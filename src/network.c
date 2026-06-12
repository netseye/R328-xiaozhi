#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef XZ_TARGET_DEVICE

#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define WS_FIN_BIT    0x80
#define WS_OPCODE_TEXT  0x01
#define WS_OPCODE_BIN   0x02
#define WS_OPCODE_CLOSE 0x08
#define WS_OPCODE_PING  0x09
#define WS_OPCODE_PONG  0x0A

struct xz_net_t {
    int fd;
    SSL *ssl;
    SSL_CTX *ssl_ctx;
    xz_net_config_t config;
    xz_net_callbacks_t cbs;
    int connected;
    uint8_t recv_buf[8192];
    int recv_len;
};

static int ws_parse_url(const char *url, char *host, int host_sz,
                        char *path, int path_sz, int *use_ssl, int *port) {
    const char *p = url;
    *use_ssl = 0;
    *port = 80;

    if (strncmp(p, "wss://", 6) == 0) {
        *use_ssl = 1;
        *port = 443;
        p += 6;
    } else if (strncmp(p, "ws://", 5) == 0) {
        p += 5;
    } else {
        return -1;
    }

    const char *path_p = strchr(p, '/');
    const char *port_p = strchr(p, ':');

    if (port_p && (!path_p || port_p < path_p)) {
        int hlen = port_p - p;
        if (hlen >= host_sz) hlen = host_sz - 1;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
        *port = atoi(port_p + 1);
        p = port_p + 1;
    } else {
        const char *end = path_p ? path_p : (p + strlen(p));
        int hlen = end - p;
        if (hlen >= host_sz) hlen = host_sz - 1;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
    }

    if (path_p) {
        strncpy(path, path_p, path_sz - 1);
        path[path_sz - 1] = '\0';
    } else {
        strcpy(path, "/");
    }
    return 0;
}

static int tcp_connect(const char *host, int port) {
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0)
        return -1;

    int fd = -1;
    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

static void base64_encode(const uint8_t *in, int len, char *out) {
    static const char t[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i;
    for (i = 0; i < len - 2; i += 3) {
        *out++ = t[(in[i] >> 2) & 0x3F];
        *out++ = t[((in[i] & 0x3) << 4) | ((in[i+1] >> 4) & 0xF)];
        *out++ = t[((in[i+1] & 0xF) << 2) | ((in[i+2] >> 6) & 0x3)];
        *out++ = t[in[i+2] & 0x3F];
    }
    if (i < len) {
        *out++ = t[(in[i] >> 2) & 0x3F];
        if (i + 1 < len) {
            *out++ = t[((in[i] & 0x3) << 4) | ((in[i+1] >> 4) & 0xF)];
            *out++ = t[((in[i+1] & 0xF) << 2)];
        } else {
            *out++ = t[((in[i] & 0x3) << 4)];
            *out++ = '=';
        }
        *out++ = '=';
    }
    *out = '\0';
}

static int ws_do_handshake(xz_net_t *n) {
    char host[256], path[512];
    int use_ssl, port;
    if (ws_parse_url(n->config.url, host, sizeof(host),
                     path, sizeof(path), &use_ssl, &port) < 0)
        return -1;

    n->fd = tcp_connect(host, port);
    if (n->fd < 0) return -1;

    if (use_ssl) {
        n->ssl_ctx = SSL_CTX_new(SSLv23_client_method());
        if (!n->ssl_ctx) return -1;
        n->ssl = SSL_new(n->ssl_ctx);
        SSL_set_fd(n->ssl, n->fd);
        SSL_set_tlsext_host_name(n->ssl, host);
        if (SSL_connect(n->ssl) != 1) return -1;
    }

    uint8_t nonce[16];
    for (int i = 0; i < 16; i++) nonce[i] = rand() & 0xFF;
    char nonce_b64[32];
    base64_encode(nonce, 16, nonce_b64);

    char req[2048];
    int rlen = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Authorization: %s\r\n"
        "Protocol-Version: 1\r\n"
        "Device-Id: %s\r\n"
        "Client-Id: %s\r\n"
        "\r\n",
        path, host, port, nonce_b64,
        n->config.token, n->config.device_id, n->config.client_id);

    if (use_ssl) {
        SSL_write(n->ssl, req, rlen);
    } else {
        write(n->fd, req, rlen);
    }

    char resp[1024];
    int rlen2;
    if (use_ssl) {
        rlen2 = SSL_read(n->ssl, resp, sizeof(resp) - 1);
    } else {
        rlen2 = read(n->fd, resp, sizeof(resp) - 1);
    }
    if (rlen2 <= 0) return -1;
    resp[rlen2] = '\0';

    if (strncmp(resp, "HTTP/1.1 101", 12) != 0 &&
        strncmp(resp, "HTTP/1.0 101", 12) != 0)
        return -1;

    char *body = strstr(resp, "\r\n\r\n");
    if (!body) return -1;
    body += 4;
    int header_len = body - resp;
    int leftover = rlen2 - header_len;
    if (leftover > 0) {
        memcpy(n->recv_buf, body, leftover);
        n->recv_len = leftover;
    }

    n->connected = 1;
    return 0;
}

static int net_read(xz_net_t *n, uint8_t *buf, int len) {
    if (n->ssl) return SSL_read(n->ssl, buf, len);
    return read(n->fd, buf, len);
}

static int net_write(xz_net_t *n, const uint8_t *buf, int len) {
    if (n->ssl) return SSL_write(n->ssl, buf, len);
    return write(n->fd, buf, len);
}

static void ws_handle_frame(xz_net_t *n, int opcode, const uint8_t *data, int len) {
    switch (opcode) {
    case WS_OPCODE_TEXT:
        if (n->cbs.on_text) {
            char *text = malloc(len + 1);
            if (text) {
                memcpy(text, data, len);
                text[len] = '\0';
                n->cbs.on_text(text, n->cbs.user_data);
                free(text);
            }
        }
        break;
    case WS_OPCODE_BIN:
        if (n->cbs.on_binary)
            n->cbs.on_binary(data, len, n->cbs.user_data);
        break;
    case WS_OPCODE_PING: {
        uint8_t pong[2] = { WS_FIN_BIT | WS_OPCODE_PONG, (uint8_t)len };
        net_write(n, pong, 2);
        if (len > 0) net_write(n, data, len);
        break;
    }
    case WS_OPCODE_CLOSE:
        n->connected = 0;
        if (n->cbs.on_disconnect) n->cbs.on_disconnect(n->cbs.user_data);
        break;
    }
}

xz_net_t *xz_net_create(const xz_net_config_t *cfg, const xz_net_callbacks_t *cbs) {
    xz_net_t *n = calloc(1, sizeof(*n));
    if (!n) return NULL;
    memcpy(&n->config, cfg, sizeof(n->config));
    memcpy(&n->cbs, cbs, sizeof(n->cbs));
    n->fd = -1;
    return n;
}

int xz_net_connect(xz_net_t *n) {
    if (!n) return -1;
    return ws_do_handshake(n);
}

void xz_net_service(xz_net_t *n, int timeout_ms) {
    if (!n || !n->connected) return;

    struct timeval tv = { .tv_sec = timeout_ms / 1000,
                          .tv_usec = (timeout_ms % 1000) * 1000 };
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(n->fd, &rfds);
    if (select(n->fd + 1, &rfds, NULL, NULL, &tv) <= 0) return;

    while (n->connected) {
        if (n->recv_len < 2) break;

        uint8_t *p = n->recv_buf;
        int fin = p[0] & WS_FIN_BIT;
        int opcode = p[0] & 0x0F;
        int masked = p[1] & 0x80;
        uint64_t payload_len = p[1] & 0x7F;
        int header = 2;

        if (payload_len == 126) {
            if (n->recv_len < 4) break;
            payload_len = (p[2] << 8) | p[3];
            header = 4;
        } else if (payload_len == 127) {
            if (n->recv_len < 10) break;
            payload_len = 0;
            for (int i = 0; i < 8; i++)
                payload_len = (payload_len << 8) | p[2 + i];
            header = 10;
        }

        if (masked) header += 4;
        if (n->recv_len < header + (int)payload_len) break;

        uint8_t mask[4] = {0};
        if (masked) memcpy(mask, p + header - 4, 4);

        uint8_t *payload = p + header;
        for (uint64_t i = 0; masked && i < payload_len; i++)
            payload[i] ^= mask[i & 3];

        ws_handle_frame(n, opcode, payload, (int)payload_len);

        int frame_total = header + (int)payload_len;
        int remaining = n->recv_len - frame_total;
        if (remaining > 0)
            memmove(n->recv_buf, n->recv_buf + frame_total, remaining);
        n->recv_len = remaining;

        if (!fin) continue;
        break;
    }

    if (n->recv_len < (int)sizeof(n->recv_buf) - 256) {
        int r = net_read(n, n->recv_buf + n->recv_len,
                         sizeof(n->recv_buf) - n->recv_len);
        if (r > 0) n->recv_len += r;
    }
}

int xz_net_send_binary(xz_net_t *n, const uint8_t *data, int len) {
    if (!n || !n->connected) return -1;
    uint8_t hdr[10];
    int hdrlen = 0;
    hdr[0] = WS_FIN_BIT | WS_OPCODE_BIN;
    if (len < 126) {
        hdr[1] = len;
        hdrlen = 2;
    } else if (len < 65536) {
        hdr[1] = 126;
        hdr[2] = (len >> 8) & 0xFF;
        hdr[3] = len & 0xFF;
        hdrlen = 4;
    } else {
        hdr[1] = 127;
        for (int i = 0; i < 8; i++)
            hdr[9 - i] = (len >> (i * 8)) & 0xFF;
        hdrlen = 10;
    }
    net_write(n, hdr, hdrlen);
    return net_write(n, data, len) == len ? 0 : -1;
}

int xz_net_send_text(xz_net_t *n, const char *json) {
    if (!n || !n->connected) return -1;
    int len = strlen(json);
    uint8_t hdr[10];
    int hdrlen = 0;
    hdr[0] = WS_FIN_BIT | WS_OPCODE_TEXT;
    if (len < 126) {
        hdr[1] = len;
        hdrlen = 2;
    } else if (len < 65536) {
        hdr[1] = 126;
        hdr[2] = (len >> 8) & 0xFF;
        hdr[3] = len & 0xFF;
        hdrlen = 4;
    } else {
        return -1;
    }
    net_write(n, hdr, hdrlen);
    return net_write(n, (const uint8_t *)json, len) == len ? 0 : -1;
}

void xz_net_disconnect(xz_net_t *n) {
    if (!n) return;
    if (n->connected) {
        uint8_t close_frame[2] = { WS_FIN_BIT | WS_OPCODE_CLOSE, 0 };
        net_write(n, close_frame, 2);
    }
    n->connected = 0;
    if (n->ssl) {
        SSL_shutdown(n->ssl);
        SSL_free(n->ssl);
        n->ssl = NULL;
    }
    if (n->ssl_ctx) {
        SSL_CTX_free(n->ssl_ctx);
        n->ssl_ctx = NULL;
    }
    if (n->fd >= 0) {
        close(n->fd);
        n->fd = -1;
    }
}

void xz_net_destroy(xz_net_t *n) {
    if (!n) return;
    xz_net_disconnect(n);
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
