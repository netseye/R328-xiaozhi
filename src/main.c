#include "config.h"
#include "audio.h"
#include "network.h"
#include "protocol.h"
#include "state_machine.h"
#include "cli.h"
#include "ota.h"
#include "wake_word.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static volatile int g_running = 1;
static char g_config_path[512] = "config.json";

static void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

typedef struct {
    xz_config_t cfg;
    xz_audio_t *audio;
    xz_net_t *net;
    xz_sm_t sm;
    xz_wake_t *wake;
    char session_id[128];
} app_t;

static app_t g_app;

static const char *state_name(xz_state_t s) {
    switch (s) {
    case XZ_STATE_IDLE:       return "IDLE";
    case XZ_STATE_CONNECTING: return "CONNECTING";
    case XZ_STATE_LISTENING:  return "LISTENING";
    case XZ_STATE_SPEAKING:   return "SPEAKING";
    }
    return "UNKNOWN";
}

static void on_transition(xz_state_t from, xz_state_t to, void *user_data) {
    (void)user_data;
    printf("[STATE] %s -> %s\n", state_name(from), state_name(to));
}

static void handle_hello_ack(const char *json) {
    xz_hello_ack_t ack;
    if (xz_proto_parse_hello_ack(json, &ack) == 0) {
        strncpy(g_app.session_id, ack.session_id, sizeof(g_app.session_id) - 1);
        printf("[SESSION] %s\n", ack.session_id);
        xz_sm_event(&g_app.sm, XZ_EVENT_HELLO_ACK);

        char listen_msg[512];
        xz_proto_build_listen(listen_msg, sizeof(listen_msg),
                              g_app.session_id, "start", "auto");
        printf("[NET] TX listen: %s\n", listen_msg);
        xz_net_send_text(g_app.net, listen_msg);

        xz_audio_start_capture(g_app.audio);
    }
}

static void on_net_connect(void *user_data) {
    (void)user_data;
    printf("[NET] Connected\n");
    char buf[512];
    xz_proto_build_hello(buf, sizeof(buf));
    printf("[NET] TX: %s\n", buf);
    xz_net_send_text(g_app.net, buf);
}

static void on_net_binary(const uint8_t *data, int len, void *user_data) {
    (void)user_data;
    xz_audio_play_opus(g_app.audio, data, len);
}

static void on_net_text(const char *json, void *user_data) {
    (void)user_data;
    printf("[NET] RX: %s\n", json);
    xz_msg_type_t type = xz_proto_parse_type(json);

    switch (type) {
    case XZ_MSG_HELLO:
        handle_hello_ack(json);
        break;
    case XZ_MSG_STT: {
        xz_stt_t stt;
        if (xz_proto_parse_stt(json, &stt) == 0) {
            printf("[STT] %s\n", stt.text);
        }
        break;
    }
    case XZ_MSG_TTS_START:
        xz_audio_stop_capture(g_app.audio);
        xz_sm_event(&g_app.sm, XZ_EVENT_TTS_START);
        xz_audio_start_playback(g_app.audio);
        printf("[TTS] Speaking...\n");
        break;
    case XZ_MSG_TTS_STOP:
        xz_audio_stop_playback(g_app.audio);
        xz_sm_event(&g_app.sm, XZ_EVENT_TTS_STOP);
        printf("[TTS] Done\n");
        break;
    case XZ_MSG_LLM:
        printf("[LLM] %s\n", json);
        break;
    case XZ_MSG_MCP: {
        int mcp_id = 0;
        char mcp_method[64] = {0};
        if (xz_proto_parse_mcp_request(json, &mcp_id, mcp_method, sizeof(mcp_method)) == 0) {
            char resp[512];
            if (strcmp(mcp_method, "initialize") == 0) {
                xz_proto_build_mcp_init_result(resp, sizeof(resp), g_app.session_id, mcp_id);
                printf("[NET] TX mcp-init: %s\n", resp);
                xz_net_send_text(g_app.net, resp);
            } else if (strcmp(mcp_method, "tools/list") == 0) {
                xz_proto_build_mcp_tools_result(resp, sizeof(resp), g_app.session_id, mcp_id);
                printf("[NET] TX mcp-tools: %s\n", resp);
                xz_net_send_text(g_app.net, resp);
            }
        }
        break;
    }
    default:
        printf("[MSG] %s\n", json);
        break;
    }
}

static void on_net_disconnect(void *user_data) {
    (void)user_data;
    printf("[NET] Disconnected\n");
    xz_audio_stop_capture(g_app.audio);
    xz_audio_stop_playback(g_app.audio);
    xz_sm_event(&g_app.sm, XZ_EVENT_DISCONNECTED);
}

static int do_ota_registration(void) {
    xz_ota_result_t ota;
    int rc = xz_ota_register(&g_app.cfg, &ota);
    if (rc != 0) {
        printf("[OTA] Registration failed\n");
        return -1;
    }

    if (ota.activated && ota.token[0]) {
        strncpy(g_app.cfg.xiaozhi_token, ota.token, sizeof(g_app.cfg.xiaozhi_token) - 1);
        if (ota.websocket_url[0]) {
            strncpy(g_app.cfg.xiaozhi_url, ota.websocket_url, sizeof(g_app.cfg.xiaozhi_url) - 1);
        }
        xz_config_save_token(g_config_path, ota.token);
        printf("[OTA] Token saved to config: %.20s...\n", ota.token);
        printf("[OTA] xiaozhi_token now: %.20s...\n", g_app.cfg.xiaozhi_token);
        return 0;
    }

    if (ota.activation_code[0]) {
        printf("[OTA] Waiting for device binding... (Ctrl+C to cancel)\n");
        for (int i = 0; i < 60 && g_running; i++) {
            sleep(3);
            /* Call /activate to confirm binding */
            int act_rc = xz_ota_activate(&g_app.cfg);
            if (act_rc == 0) {
                /* Activation confirmed, now re-register to get token */
                memset(&ota, 0, sizeof(ota));
                rc = xz_ota_register(&g_app.cfg, &ota);
                if (rc == 0 && ota.activated && ota.token[0]) {
                    strncpy(g_app.cfg.xiaozhi_token, ota.token,
                            sizeof(g_app.cfg.xiaozhi_token) - 1);
                    if (ota.websocket_url[0]) {
                        strncpy(g_app.cfg.xiaozhi_url, ota.websocket_url,
                                sizeof(g_app.cfg.xiaozhi_url) - 1);
                    }
                    xz_config_save_token(g_config_path, ota.token);
                    printf("[OTA] Device activated, token saved\n");
                    return 0;
                }
            }
            printf("[OTA] Still waiting... (%d/60)\n", i + 1);
        }
        printf("[OTA] Timeout waiting for binding\n");
        return -1;
    }

    printf("[OTA] Unexpected response\n");
    return -1;
}

static void start_conversation(void) {
    if (xz_sm_current(&g_app.sm) != XZ_STATE_IDLE) {
        printf("Already in state: %s\n", state_name(xz_sm_current(&g_app.sm)));
        return;
    }

    xz_sm_event(&g_app.sm, XZ_EVENT_USER_TRIGGER);

    printf("[WS] Using token: %.20s... url=%s\n", g_app.cfg.xiaozhi_token, g_app.cfg.xiaozhi_url);
    xz_net_config_t ncfg;
    strncpy(ncfg.url, g_app.cfg.xiaozhi_url, sizeof(ncfg.url) - 1);
    strncpy(ncfg.token, g_app.cfg.xiaozhi_token, sizeof(ncfg.token) - 1);
    strncpy(ncfg.device_id, g_app.cfg.device_id, sizeof(ncfg.device_id) - 1);
    strncpy(ncfg.client_id, g_app.cfg.client_id, sizeof(ncfg.client_id) - 1);

    xz_net_callbacks_t cbs = {
        .on_connect = on_net_connect,
        .on_binary = on_net_binary,
        .on_text = on_net_text,
        .on_disconnect = on_net_disconnect,
        .user_data = &g_app
    };

    if (g_app.net) xz_net_destroy(g_app.net);
    g_app.net = xz_net_create(&ncfg, &cbs);
    if (!g_app.net) {
        printf("Failed to create network\n");
        xz_sm_event(&g_app.sm, XZ_EVENT_DISCONNECTED);
        return;
    }

    if (xz_net_connect(g_app.net) != 0) {
        printf("Failed to connect\n");
        xz_sm_event(&g_app.sm, XZ_EVENT_DISCONNECTED);
    }
}

static void stop_conversation(void) {
    if (g_app.net) xz_net_disconnect(g_app.net);
    xz_audio_stop_capture(g_app.audio);
    xz_audio_stop_playback(g_app.audio);
    if (g_app.wake) xz_wake_reset(g_app.wake);
    xz_sm_event(&g_app.sm, XZ_EVENT_ABORT);
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            strncpy(g_config_path, argv[++i], sizeof(g_config_path) - 1);
        }
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (xz_config_load_file(g_config_path, &g_app.cfg) != 0) {
        fprintf(stderr, "Failed to load config: %s\n", g_config_path);
        return 1;
    }
    printf("Config loaded from %s\n", g_config_path);
    printf("  Device: %s\n", g_app.cfg.device_id);
    printf("  OTA URL: %s\n", g_app.cfg.ota_url);

    if (!g_app.cfg.xiaozhi_token[0]) {
        printf("[OTA] No valid token, starting OTA registration...\n");
        if (do_ota_registration() != 0) {
            printf("[OTA] Registration failed. Use 'ota' command to retry.\n");
        }
    }

    xz_sm_init(&g_app.sm, on_transition, NULL);
    if (strcmp(g_app.cfg.mode, "auto") == 0)
        g_app.sm.mode = XZ_MODE_AUTO;

    xz_audio_config_t acfg = {
        .sample_rate = g_app.cfg.audio_sample_rate,
        .output_sample_rate = g_app.cfg.audio_output_sample_rate,
        .channels = g_app.cfg.audio_channels,
        .frame_ms = g_app.cfg.audio_frame_duration_ms,
    };
    strncpy(acfg.capture_dev, g_app.cfg.audio_capture_device, sizeof(acfg.capture_dev) - 1);
    strncpy(acfg.playback_dev, g_app.cfg.audio_playback_device, sizeof(acfg.playback_dev) - 1);

    g_app.audio = xz_audio_create(&acfg);
    if (!g_app.audio) {
        fprintf(stderr, "Failed to init audio\n");
        return 1;
    }

    /* Initialize wake word detector (if model file exists) */
    if (g_app.cfg.wake_word_model[0]) {
        xz_wake_config_t wcfg = {
            .sample_rate = g_app.cfg.audio_sample_rate,
            .model_path = g_app.cfg.wake_word_model,
            .probability_cutoff = 0.05f,
            .sliding_window_size = 3,
            .feature_step_ms = 10,
        };
        g_app.wake = xz_wake_create(&wcfg);
        if (g_app.wake) {
            printf("[WAKE] Wake word detection enabled: %s\n", g_app.cfg.wake_word_model);
        } else {
            printf("[WAKE] Wake word init failed, continuing without\n");
        }
    }

    xz_cli_init();
    printf("XiaoZhi R328 Client ready. Type 'help' for commands.\n");

    while (g_running) {
        if (g_app.net) {
            xz_net_service(g_app.net, 10);
        }

        if (xz_sm_current(&g_app.sm) == XZ_STATE_LISTENING && g_app.net) {
            uint8_t opus_buf[XZ_OPUS_MAX_BYTES];
            int n = xz_audio_read_opus(g_app.audio, opus_buf, sizeof(opus_buf));
            if (n > 0) {
                xz_net_send_binary(g_app.net, opus_buf, n);
            }
        }

        /* Wake word detection in IDLE state */
        if (g_app.wake && xz_sm_current(&g_app.sm) == XZ_STATE_IDLE) {
            /* Start capture if not already running */
            if (!g_app.audio) {
                /* shouldn't happen */
            } else {
                static int wake_capturing = 0;
                if (!wake_capturing) {
                    xz_audio_start_capture(g_app.audio);
                    wake_capturing = 1;
                }
                int16_t pcm_buf[160]; /* 10ms at 16kHz */
                int n = xz_audio_read_pcm(g_app.audio, pcm_buf, 160);
                if (n > 0) {
                    /* Debug: log audio level every 100 reads */
                    static int audio_dbg = 0;
                    audio_dbg++;
                    if (audio_dbg % 100 == 0) {
                        int32_t sum = 0;
                        for (int i = 0; i < n; i++) sum += abs(pcm_buf[i]);
                        fprintf(stderr, "[AUDIO] %d samples, avg_level=%d\n", n, sum / n);
                    }
                    int detected = xz_wake_process(g_app.wake, pcm_buf, n);
                    if (detected == 1) {
                        xz_audio_stop_capture(g_app.audio);
                        wake_capturing = 0;
                        xz_wake_reset(g_app.wake);
                        printf("[WAKE] Triggering conversation!\n");
                        start_conversation();
                    }
                }
            }
        }

        xz_cli_command_t cmd = xz_cli_read();
        switch (cmd.cmd) {
        case XZ_CMD_CHAT:
            start_conversation();
            break;
        case XZ_CMD_TEXT:
            printf("Text mode not yet implemented (needs protocol extension)\n");
            break;
        case XZ_CMD_STOP:
            stop_conversation();
            break;
        case XZ_CMD_STATUS:
            printf("State: %s\n", state_name(xz_sm_current(&g_app.sm)));
            break;
        case XZ_CMD_VOLUME:
            xz_audio_set_volume(g_app.audio, atoi(cmd.arg));
            printf("Volume set to %s\n", cmd.arg);
            break;
        case XZ_CMD_OTA:
            if (do_ota_registration() == 0) {
                printf("[OTA] Registration complete\n");
            }
            break;
        case XZ_CMD_QUIT:
            g_running = 0;
            break;
        default:
            break;
        }

        usleep(1000);
    }

    stop_conversation();
    if (g_app.net) xz_net_destroy(g_app.net);
    if (g_app.audio) xz_audio_destroy(g_app.audio);
    if (g_app.wake) xz_wake_destroy(g_app.wake);

    printf("Goodbye!\n");
    return 0;
}
