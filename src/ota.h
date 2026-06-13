#ifndef XZ_OTA_H
#define XZ_OTA_H

#include "config.h"

typedef struct {
    char activation_code[16];
    char token[256];
    char websocket_url[256];
    int activated;
} xz_ota_result_t;

int xz_ota_get_mac(char *buf, int bufsize);
int xz_ota_register(const xz_config_t *cfg, xz_ota_result_t *result);
int xz_ota_activate(const xz_config_t *cfg);

#endif
