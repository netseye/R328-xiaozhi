#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef XZ_TARGET_DEVICE

#include <libwebsockets.h>

struct xz_net_t {
    struct lws_context *context;
    struct lws *wsi;
    xz_net_config_t config;
    xz_net_callbacks_t cbs;
    int connected;
    uint8_t send_buf[4096];
    int send_len;
    int send_is_binary;
    int has_pending_send;
};

static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
                       void *user, void *in, size_t len) {
    (void)user;
    xz_net_t *n = (xz_net_t *)lws_wsi_user(wsi);
    if (!n) return 0;

    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        n->connected = 1;
        if (n->cbs.on_connect) n->cbs.on_connect(n->cbs.user_data);
        break;
    case LWS_CALLBACK_CLIENT_RECEIVE: {
        int is_binary = lws_frame_is_binary(wsi);
        if (is_binary && n->cbs.on_binary)
            n->cbs.on_binary((const uint8_t *)in, (int)len, n->cbs.user_data);
        else if (!is_binary && n->cbs.on_text)
            n->cbs.on_text((const char *)in, n->cbs.user_data);
        break;
    }
    case LWS_CALLBACK_CLIENT_WRITEABLE:
        if (n->has_pending_send) {
            unsigned char buf[LWS_PRE + 4096];
            memcpy(buf + LWS_PRE, n->send_buf, n->send_len);
            enum lws_write_protocol wp = n->send_is_binary ?
                LWS_WRITE_BINARY : LWS_WRITE_TEXT;
            lws_write(wsi, buf + LWS_PRE, n->send_len, wp);
            n->has_pending_send = 0;
        }
        break;
    case LWS_CALLBACK_CLIENT_CLOSED:
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        n->connected = 0;
        n->wsi = NULL;
        if (n->cbs.on_disconnect) n->cbs.on_disconnect(n->cbs.user_data);
        break;
    default:
        break;
    }
    return 0;
}

static const struct lws_protocols protocols[] = {
    {"xiaozhi-ws", ws_callback, 0, 0},
    {NULL, NULL, 0, 0}
};

xz_net_t *xz_net_create(const xz_net_config_t *cfg, const xz_net_callbacks_t *cbs) {
    xz_net_t *n = calloc(1, sizeof(*n));
    if (!n) return NULL;
    memcpy(&n->config, cfg, sizeof(n->config));
    memcpy(&n->cbs, cbs, sizeof(n->cbs));

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    n->context = lws_create_context(&info);
    if (!n->context) { free(n); return NULL; }
    return n;
}

int xz_net_connect(xz_net_t *n) {
    if (!n) return -1;

    const char *url = n->config.url;
    int ssl = 0;
    const char *host_start = url;
    if (strncmp(url, "wss://", 6) == 0) { ssl = 1; host_start = url + 6; }
    else if (strncmp(url, "ws://", 5) == 0) { host_start = url + 5; }

    char host[256] = {0};
    const char *path_start = strchr(host_start, '/');
    if (path_start) {
        memcpy(host, host_start, path_start - host_start);
    } else {
        strcpy(host, host_start);
    }

    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = n->context;
    ccinfo.address = host;
    ccinfo.path = path_start ? path_start : "/";
    ccinfo.host = host;
    ccinfo.origin = host;
    ccinfo.protocol = "xiaozhi-ws";
    ccinfo.userdata = n;
    ccinfo.port = ssl ? 443 : 80;
    if (ssl) ccinfo.ssl_connection = LCCSCF_USE_SSL;

    n->wsi = lws_client_connect_via_info(&ccinfo);
    return n->wsi ? 0 : -1;
}

void xz_net_service(xz_net_t *n, int timeout_ms) {
    if (n && n->context) lws_service(n->context, timeout_ms);
}

int xz_net_send_binary(xz_net_t *n, const uint8_t *data, int len) {
    if (!n || !n->connected || len > (int)sizeof(n->send_buf)) return -1;
    memcpy(n->send_buf, data, len);
    n->send_len = len;
    n->send_is_binary = 1;
    n->has_pending_send = 1;
    if (n->wsi) lws_callback_on_writable(n->wsi);
    return 0;
}

int xz_net_send_text(xz_net_t *n, const char *json) {
    if (!n || !n->connected) return -1;
    int len = strlen(json);
    if (len > (int)sizeof(n->send_buf)) return -1;
    memcpy(n->send_buf, json, len);
    n->send_len = len;
    n->send_is_binary = 0;
    n->has_pending_send = 1;
    if (n->wsi) lws_callback_on_writable(n->wsi);
    return 0;
}

void xz_net_disconnect(xz_net_t *n) {
    if (n && n->wsi) {
        lws_wsi_close(n->wsi);
        n->wsi = NULL;
        n->connected = 0;
    }
}

void xz_net_destroy(xz_net_t *n) {
    if (!n) return;
    xz_net_disconnect(n);
    if (n->context) lws_context_destroy(n->context);
    free(n);
}

#else /* Host stubs */

struct xz_net_t { int dummy; };

xz_net_t *xz_net_create(const xz_net_config_t *cfg, const xz_net_callbacks_t *cbs) { (void)cfg; (void)cbs; return calloc(1, sizeof(xz_net_t)); }
int xz_net_connect(xz_net_t *n) { (void)n; return 0; }
void xz_net_service(xz_net_t *n, int timeout_ms) { (void)n; (void)timeout_ms; }
int xz_net_send_binary(xz_net_t *n, const uint8_t *data, int len) { (void)n; (void)data; (void)len; return 0; }
int xz_net_send_text(xz_net_t *n, const char *json) { (void)n; (void)json; return 0; }
void xz_net_disconnect(xz_net_t *n) { (void)n; }
void xz_net_destroy(xz_net_t *n) { free(n); }

#endif
