#ifndef XZ_CONFIG_H
#define XZ_CONFIG_H

#define XZ_MAX_STR 256

typedef struct {
    char wifi_ssid[XZ_MAX_STR];
    char wifi_password[XZ_MAX_STR];
    char xiaozhi_token[XZ_MAX_STR];
    char xiaozhi_url[XZ_MAX_STR];
    char ota_url[XZ_MAX_STR];
    char device_id[XZ_MAX_STR];
    char client_id[XZ_MAX_STR];
    char audio_capture_device[XZ_MAX_STR];
    char audio_playback_device[XZ_MAX_STR];
    int  audio_sample_rate;
    int  audio_output_sample_rate;
    int  audio_channels;
    int  audio_frame_duration_ms;
    char mode[XZ_MAX_STR];
    char log_level[XZ_MAX_STR];
    char wake_word_model[XZ_MAX_STR];
    float wake_word_threshold;
} xz_config_t;

int xz_config_parse(const char *json, xz_config_t *cfg);
int xz_config_load_file(const char *path, xz_config_t *cfg);
int xz_config_save_token(const char *path, const char *token);

#endif
