#ifndef XZ_NETWORK_H
#define XZ_NETWORK_H

#include <stdint.h>

typedef struct xz_net_t xz_net_t;

typedef void (*xz_net_connect_cb)(void *user_data);
typedef void (*xz_net_binary_cb)(const uint8_t *data, int len, void *user_data);
typedef void (*xz_net_text_cb)(const char *json, void *user_data);
typedef void (*xz_net_disconnect_cb)(void *user_data);

typedef struct {
    xz_net_connect_cb on_connect;
    xz_net_binary_cb  on_binary;
    xz_net_text_cb    on_text;
    xz_net_disconnect_cb on_disconnect;
    void *user_data;
} xz_net_callbacks_t;

typedef struct {
    char url[256];
    char token[256];
    char device_id[64];
    char client_id[64];
} xz_net_config_t;

xz_net_t *xz_net_create(const xz_net_config_t *cfg, const xz_net_callbacks_t *cbs);
int xz_net_connect(xz_net_t *n);
void xz_net_service(xz_net_t *n, int timeout_ms);
int xz_net_send_binary(xz_net_t *n, const uint8_t *data, int len);
int xz_net_send_text(xz_net_t *n, const char *json);
void xz_net_disconnect(xz_net_t *n);
void xz_net_destroy(xz_net_t *n);

#endif
