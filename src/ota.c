#include "ota.h"

#ifdef XZ_TARGET_DEVICE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "cJSON.h"

int xz_ota_get_mac(char *buf, int bufsize) {
    const char *paths[] = {
        "/sys/class/net/wlan0/address",
        "/sys/class/net/eth0/address",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        FILE *f = fopen(paths[i], "r");
        if (f) {
            if (fgets(buf, bufsize, f)) {
                char *nl = strchr(buf, '\n');
                if (nl) *nl = '\0';
                fclose(f);
                return 0;
            }
            fclose(f);
        }
    }
    return -1;
}

static int https_post(const char *url, const char *body,
                      const char *device_id, const char *client_id,
                      char *resp, int resp_sz) {
    char host[256] = {0}, path[512] = {0};
    int port = 443, use_ssl = 1;

    const char *p = url;
    if (strncmp(p, "https://", 8) == 0) {
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        p += 7;
        port = 80;
        use_ssl = 0;
    }

    const char *path_p = strchr(p, '/');
    const char *port_p = strchr(p, ':');

    if (port_p && (!path_p || port_p < path_p)) {
        int hlen = port_p - p;
        if (hlen >= (int)sizeof(host)) hlen = sizeof(host) - 1;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
        port = atoi(port_p + 1);
    } else {
        const char *end = path_p ? path_p : (p + strlen(p));
        int hlen = end - p;
        if (hlen >= (int)sizeof(host)) hlen = sizeof(host) - 1;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
    }

    if (path_p) {
        strncpy(path, path_p, sizeof(path) - 1);
    } else {
        strcpy(path, "/");
    }

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);
    if (getaddrinfo(host, port_str, &hints, &res) != 0) return -1;

    int fd = -1;
    struct addrinfo *rp;
    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) return -1;

    SSL_CTX *ssl_ctx = NULL;
    SSL *ssl = NULL;

    if (use_ssl) {
        ssl_ctx = SSL_CTX_new(SSLv23_client_method());
        if (!ssl_ctx) { close(fd); return -1; }
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
        ssl = SSL_new(ssl_ctx);
        SSL_set_fd(ssl, fd);
        SSL_set_tlsext_host_name(ssl, host);
        if (SSL_connect(ssl) != 1) {
            SSL_free(ssl);
            SSL_CTX_free(ssl_ctx);
            close(fd);
            return -1;
        }
    }

    int body_len = (int)strlen(body);
    char req[2048];
    int rlen = snprintf(req, sizeof(req),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Mozilla/5.0\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Device-Id: %s\r\n"
        "Client-Id: %s\r\n"
        "Activation-Version: 1\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        path, host, body_len, device_id, client_id, body);

    if (use_ssl) {
        SSL_write(ssl, req, rlen);
    } else {
        write(fd, req, rlen);
    }

    int total = 0;
    while (total < resp_sz - 1) {
        int r;
        if (use_ssl) {
            r = SSL_read(ssl, resp + total, resp_sz - 1 - total);
        } else {
            r = (int)read(fd, resp + total, resp_sz - 1 - total);
        }
        if (r <= 0) break;
        total += r;
    }
    resp[total] = '\0';

    if (use_ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ssl_ctx);
    }
    close(fd);
    return total;
}

int xz_ota_register(const xz_config_t *cfg, xz_ota_result_t *result) {
    memset(result, 0, sizeof(*result));

    char mac[64] = {0};
    const char *device_id = cfg->device_id;

    if (!device_id[0]) {
        if (xz_ota_get_mac(mac, sizeof(mac)) != 0) {
            fprintf(stderr, "[OTA] Cannot get device MAC address\n");
            return -1;
        }
        device_id = mac;
    }

    printf("[OTA] Device: %s, Client: %s\n", device_id, cfg->client_id);

    char body[512];
    snprintf(body, sizeof(body),
        "{\"board\":\"xiaozhi-r328\",\"version\":\"1.0.0\","
        "\"flash_size\":16777216,\"minimum_free_heap\":1048576}");

    char resp[4096];
    int rc = https_post(cfg->ota_url, body, device_id,
                        cfg->client_id, resp, sizeof(resp));
    if (rc <= 0) {
        fprintf(stderr, "[OTA] HTTPS POST failed\n");
        return -1;
    }

    char *http_body = strstr(resp, "\r\n\r\n");
    if (!http_body) {
        fprintf(stderr, "[OTA] Invalid HTTP response\n");
        return -1;
    }
    http_body += 4;

    if (strncmp(resp, "HTTP/1.1 200", 12) != 0 &&
        strncmp(resp, "HTTP/1.0 200", 12) != 0 &&
        strncmp(resp, "HTTP/1.1 202", 12) != 0 &&
        strncmp(resp, "HTTP/1.0 202", 12) != 0) {
        fprintf(stderr, "[OTA] HTTP error: %.40s\n", resp);
        return -1;
    }

    printf("[OTA] Response body: %.800s\n", http_body);

    cJSON *root = cJSON_Parse(http_body);
    if (!root) {
        fprintf(stderr, "[OTA] Invalid JSON response\n");
        return -1;
    }

    const cJSON *activation = cJSON_GetObjectItemCaseSensitive(root, "activation");
    if (activation) {
        const cJSON *code = cJSON_GetObjectItemCaseSensitive(activation, "code");
        if (cJSON_IsString(code) && code->valuestring) {
            strncpy(result->activation_code, code->valuestring,
                    sizeof(result->activation_code) - 1);
            printf("[OTA] ==================================\n");
            printf("[OTA] Activation code: %s\n", result->activation_code);
            printf("[OTA] Go to https://xiaozhi.me/ to bind\n");
            printf("[OTA] ==================================\n");
            result->activated = 0;
        }
        cJSON_Delete(root);
        return 0;
    }

    const cJSON *token = cJSON_GetObjectItemCaseSensitive(root, "token");
    if (!cJSON_IsString(token) || !token->valuestring) {
        const cJSON *ws = cJSON_GetObjectItemCaseSensitive(root, "websocket");
        if (ws) {
            const cJSON *ws_token = cJSON_GetObjectItemCaseSensitive(ws, "token");
            if (cJSON_IsString(ws_token) && ws_token->valuestring)
                token = ws_token;
            const cJSON *ws_url = cJSON_GetObjectItemCaseSensitive(ws, "url");
            if (cJSON_IsString(ws_url) && ws_url->valuestring) {
                strncpy(result->websocket_url, ws_url->valuestring,
                        sizeof(result->websocket_url) - 1);
            }
        }
    }
    if (cJSON_IsString(token) && token->valuestring) {
        strncpy(result->token, token->valuestring, sizeof(result->token) - 1);
        result->activated = 1;
        printf("[OTA] Parsed token: %.20s...\n", result->token);
    }

    const cJSON *ws_url = cJSON_GetObjectItemCaseSensitive(root, "websocket_url");
    if (cJSON_IsString(ws_url) && ws_url->valuestring) {
        strncpy(result->websocket_url, ws_url->valuestring,
                sizeof(result->websocket_url) - 1);
    }

    cJSON_Delete(root);

    if (result->token[0]) {
        printf("[OTA] Device activated, token received\n");
        return 0;
    }

    printf("[OTA] No activation code or token in response\n");
    return 0;
}

int xz_ota_activate(const xz_config_t *cfg) {
    char mac[64] = {0};
    const char *device_id = cfg->device_id;

    if (!device_id[0]) {
        if (xz_ota_get_mac(mac, sizeof(mac)) != 0) return -1;
        device_id = mac;
    }

    char url[512];
    snprintf(url, sizeof(url), "%sactivate", cfg->ota_url);
    /* ensure single slash before "activate" */
    int ulen = strlen(cfg->ota_url);
    if (ulen > 0 && cfg->ota_url[ulen - 1] != '/') {
        snprintf(url, sizeof(url), "%s/activate", cfg->ota_url);
    }

    char resp[4096];
    int rc = https_post(url, "{}", device_id, cfg->client_id, resp, sizeof(resp));
    if (rc <= 0) {
        fprintf(stderr, "[OTA] Activate POST failed\n");
        return -1;
    }

    if (strncmp(resp, "HTTP/1.1 202", 12) == 0 ||
        strncmp(resp, "HTTP/1.0 202", 12) == 0) {
        printf("[OTA] Activate: device not yet confirmed (202)\n");
        return 1; /* pending */
    }

    if (strncmp(resp, "HTTP/1.1 200", 12) != 0 &&
        strncmp(resp, "HTTP/1.0 200", 12) != 0) {
        fprintf(stderr, "[OTA] Activate HTTP error: %.40s\n", resp);
        return -1;
    }

    printf("[OTA] Activate success (200)\n");
    return 0;
}

#else /* Host stubs */

#include <stdio.h>
#include <string.h>

int xz_ota_get_mac(char *buf, int bufsize) {
    (void)buf; (void)bufsize;
    return -1;
}

int xz_ota_register(const xz_config_t *cfg, xz_ota_result_t *result) {
    (void)cfg;
    memset(result, 0, sizeof(*result));
    printf("[OTA] Skipped (host build)\n");
    return 0;
}

int xz_ota_activate(const xz_config_t *cfg) {
    (void)cfg;
    printf("[OTA] Activate skipped (host build)\n");
    return 0;
}

#endif
