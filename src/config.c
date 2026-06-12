#include "config.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_str(const cJSON *obj, const char *key, char *out, int maxlen) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsString(item) || item->valuestring == NULL) return -1;
    strncpy(out, item->valuestring, maxlen - 1);
    out[maxlen - 1] = '\0';
    return 0;
}

static int read_int(const cJSON *obj, const char *key, int defval) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item)) return item->valueint;
    return defval;
}

int xz_config_parse(const char *json, xz_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cJSON *root = cJSON_Parse(json);
    if (!root) return -1;

    int ok = 0;
    ok |= read_str(root, "wifi_ssid", cfg->wifi_ssid, XZ_MAX_STR);
    ok |= read_str(root, "wifi_password", cfg->wifi_password, XZ_MAX_STR);
    ok |= read_str(root, "xiaozhi_token", cfg->xiaozhi_token, XZ_MAX_STR);
    ok |= read_str(root, "xiaozhi_url", cfg->xiaozhi_url, XZ_MAX_STR);
    ok |= read_str(root, "device_id", cfg->device_id, XZ_MAX_STR);
    ok |= read_str(root, "client_id", cfg->client_id, XZ_MAX_STR);

    if (ok != 0) { cJSON_Delete(root); return -1; }

    cfg->audio_sample_rate = 16000;
    cfg->audio_channels = 1;
    cfg->audio_frame_duration_ms = 60;
    strncpy(cfg->audio_capture_device, "hw:0,0", XZ_MAX_STR - 1);
    strncpy(cfg->audio_playback_device, "hw:0,0", XZ_MAX_STR - 1);

    const cJSON *audio = cJSON_GetObjectItemCaseSensitive(root, "audio");
    if (audio) {
        read_str(audio, "capture_device", cfg->audio_capture_device, XZ_MAX_STR);
        read_str(audio, "playback_device", cfg->audio_playback_device, XZ_MAX_STR);
        cfg->audio_sample_rate = read_int(audio, "sample_rate", 16000);
        cfg->audio_channels = read_int(audio, "channels", 1);
        cfg->audio_frame_duration_ms = read_int(audio, "frame_duration_ms", 60);
    }

    read_str(root, "mode", cfg->mode, XZ_MAX_STR);
    if (cfg->mode[0] == '\0') strncpy(cfg->mode, "manual", XZ_MAX_STR - 1);

    read_str(root, "log_level", cfg->log_level, XZ_MAX_STR);
    if (cfg->log_level[0] == '\0') strncpy(cfg->log_level, "info", XZ_MAX_STR - 1);

    cJSON_Delete(root);
    return 0;
}

int xz_config_load_file(const char *path, xz_config_t *cfg) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return -1; }
    size_t read = fread(buf, 1, len, f);
    fclose(f);
    buf[read] = '\0';
    int rc = xz_config_parse(buf, cfg);
    free(buf);
    return rc;
}
