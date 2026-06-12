#include "test_runner.h"
#include "config.h"

TEST(parse_valid_config) {
    const char *json =
        "{"
        "  \"wifi_ssid\": \"TestWiFi\","
        "  \"wifi_password\": \"pass123\","
        "  \"xiaozhi_token\": \"Bearer tok123\","
        "  \"xiaozhi_url\": \"wss://api.xiaozhi.me/v1/ws\","
        "  \"device_id\": \"AA:BB:CC:DD:EE:FF\","
        "  \"client_id\": \"r328-0001\","
        "  \"audio\": {"
        "    \"capture_device\": \"hw:0,0\","
        "    \"playback_device\": \"hw:0,0\","
        "    \"sample_rate\": 16000,"
        "    \"channels\": 1,"
        "    \"frame_duration_ms\": 60"
        "  },"
        "  \"mode\": \"manual\","
        "  \"log_level\": \"info\""
        "}";
    xz_config_t cfg;
    int rc = xz_config_parse(json, &cfg);
    ASSERT_EQ_INT(rc, 0);
    ASSERT_EQ_STR(cfg.wifi_ssid, "TestWiFi");
    ASSERT_EQ_STR(cfg.wifi_password, "pass123");
    ASSERT_EQ_STR(cfg.xiaozhi_token, "Bearer tok123");
    ASSERT_EQ_STR(cfg.xiaozhi_url, "wss://api.xiaozhi.me/v1/ws");
    ASSERT_EQ_STR(cfg.device_id, "AA:BB:CC:DD:EE:FF");
    ASSERT_EQ_STR(cfg.client_id, "r328-0001");
    ASSERT_EQ_INT(cfg.audio_sample_rate, 16000);
    ASSERT_EQ_INT(cfg.audio_channels, 1);
    ASSERT_EQ_INT(cfg.audio_frame_duration_ms, 60);
    ASSERT_EQ_STR(cfg.audio_capture_device, "hw:0,0");
    ASSERT_EQ_STR(cfg.audio_playback_device, "hw:0,0");
    ASSERT_EQ_STR(cfg.mode, "manual");
    ASSERT_EQ_STR(cfg.log_level, "info");
}

TEST(parse_invalid_json) {
    xz_config_t cfg;
    int rc = xz_config_parse("{invalid", &cfg);
    ASSERT_EQ_INT(rc, -1);
}

TEST(parse_missing_fields) {
    const char *json = "{\"wifi_ssid\": \"TestWiFi\"}";
    xz_config_t cfg;
    int rc = xz_config_parse(json, &cfg);
    ASSERT_EQ_INT(rc, -1);
}

TEST(parse_defaults) {
    const char *json =
        "{"
        "  \"wifi_ssid\": \"W\","
        "  \"wifi_password\": \"P\","
        "  \"xiaozhi_token\": \"Bearer T\","
        "  \"xiaozhi_url\": \"wss://example.com\","
        "  \"device_id\": \"D\","
        "  \"client_id\": \"C\""
        "}";
    xz_config_t cfg;
    int rc = xz_config_parse(json, &cfg);
    ASSERT_EQ_INT(rc, 0);
    ASSERT_EQ_INT(cfg.audio_sample_rate, 16000);
    ASSERT_EQ_INT(cfg.audio_channels, 1);
    ASSERT_EQ_INT(cfg.audio_frame_duration_ms, 60);
}

TEST(load_from_file) {
    const char *tmp = "/tmp/xz_test_config.json";
    FILE *f = fopen(tmp, "w");
    fputs("{\"wifi_ssid\":\"S\",\"wifi_password\":\"P\","
           "\"xiaozhi_token\":\"Bearer T\",\"xiaozhi_url\":\"wss://x\","
           "\"device_id\":\"D\",\"client_id\":\"C\"}", f);
    fclose(f);
    xz_config_t cfg;
    int rc = xz_config_load_file(tmp, &cfg);
    ASSERT_EQ_INT(rc, 0);
    ASSERT_EQ_STR(cfg.wifi_ssid, "S");
    remove(tmp);
}

static void run_tests(void) {
    RUN_TEST(parse_valid_config);
    RUN_TEST(parse_invalid_json);
    RUN_TEST(parse_missing_fields);
    RUN_TEST(parse_defaults);
    RUN_TEST(load_from_file);
}

TEST_MAIN()
