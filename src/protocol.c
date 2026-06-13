#include "protocol.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>

int xz_proto_build_hello(char *buf, int bufsize) {
    int len = snprintf(buf, bufsize,
        "{\"type\":\"hello\",\"version\":1,\"transport\":\"websocket\","
        "\"features\":{\"mcp\":true},"
        "\"audio_params\":{\"format\":\"opus\",\"sample_rate\":16000,"
        "\"channels\":1,\"frame_duration\":60}}");
    return (len >= bufsize) ? -1 : len;
}

int xz_proto_build_listen(char *buf, int bufsize, const char *session_id,
                           const char *state, const char *mode) {
    int len;
    if (mode) {
        len = snprintf(buf, bufsize,
            "{\"session_id\":\"%s\",\"type\":\"listen\",\"state\":\"%s\",\"mode\":\"%s\"}",
            session_id, state, mode);
    } else {
        len = snprintf(buf, bufsize,
            "{\"session_id\":\"%s\",\"type\":\"listen\",\"state\":\"%s\"}",
            session_id, state);
    }
    return (len >= bufsize) ? -1 : len;
}

int xz_proto_build_abort(char *buf, int bufsize, const char *session_id,
                          const char *reason) {
    int len = snprintf(buf, bufsize,
        "{\"session_id\":\"%s\",\"type\":\"abort\",\"reason\":\"%s\"}",
        session_id, reason);
    return (len >= bufsize) ? -1 : len;
}

static int read_str_field(const cJSON *obj, const char *key, char *out, int maxlen) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsString(item) || !item->valuestring) return -1;
    strncpy(out, item->valuestring, maxlen - 1);
    out[maxlen - 1] = '\0';
    return 0;
}

xz_msg_type_t xz_proto_parse_type(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return XZ_MSG_UNKNOWN;

    const cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    const cJSON *state = cJSON_GetObjectItemCaseSensitive(root, "state");
    xz_msg_type_t result = XZ_MSG_UNKNOWN;

    if (cJSON_IsString(type) && type->valuestring) {
        const char *t = type->valuestring;

        if (strcmp(t, "hello") == 0) {
            result = XZ_MSG_HELLO;
        } else if (strcmp(t, "stt") == 0) {
            result = XZ_MSG_STT;
        } else if (strcmp(t, "tts") == 0) {
            if (cJSON_IsString(state) && state->valuestring) {
                if (strcmp(state->valuestring, "start") == 0)
                    result = XZ_MSG_TTS_START;
                else if (strcmp(state->valuestring, "stop") == 0)
                    result = XZ_MSG_TTS_STOP;
                else if (strcmp(state->valuestring, "sentence_start") == 0)
                    result = XZ_MSG_TTS_SENTENCE;
                else
                    result = XZ_MSG_UNKNOWN;
            }
        } else if (strcmp(t, "llm") == 0) {
            result = XZ_MSG_LLM;
        } else if (strcmp(t, "mcp") == 0) {
            result = XZ_MSG_MCP;
        } else if (strcmp(t, "system") == 0) {
            result = XZ_MSG_SYSTEM;
        } else if (strcmp(t, "alert") == 0) {
            result = XZ_MSG_ALERT;
        } else if (strcmp(t, "listen") == 0) {
            result = XZ_MSG_LISTEN;
        } else if (strcmp(t, "abort") == 0) {
            result = XZ_MSG_ABORT;
        }
    }

    cJSON_Delete(root);
    return result;
}

int xz_proto_parse_hello_ack(const char *json, xz_hello_ack_t *ack) {
    memset(ack, 0, sizeof(*ack));
    cJSON *root = cJSON_Parse(json);
    if (!root) return -1;

    read_str_field(root, "session_id", ack->session_id, XZ_MAX_SESSION);

    const cJSON *ap = cJSON_GetObjectItemCaseSensitive(root, "audio_params");
    if (ap) {
        const cJSON *sr = cJSON_GetObjectItemCaseSensitive(ap, "sample_rate");
        if (cJSON_IsNumber(sr)) ack->audio_sample_rate = sr->valueint;
        read_str_field(ap, "format", ack->audio_format, sizeof(ack->audio_format));
    }

    cJSON_Delete(root);
    return 0;
}

int xz_proto_parse_stt(const char *json, xz_stt_t *stt) {
    memset(stt, 0, sizeof(*stt));
    cJSON *root = cJSON_Parse(json);
    if (!root) return -1;

    read_str_field(root, "session_id", stt->session_id, XZ_MAX_SESSION);
    int rc = read_str_field(root, "text", stt->text, XZ_MAX_TEXT);

    cJSON_Delete(root);
    return rc;
}

int xz_proto_parse_mcp_init_id(const char *json, int *out_id) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return -1;

    const cJSON *payload = cJSON_GetObjectItemCaseSensitive(root, "payload");
    if (!payload) { cJSON_Delete(root); return -1; }

    const cJSON *id = cJSON_GetObjectItemCaseSensitive(payload, "id");
    if (cJSON_IsNumber(id)) {
        *out_id = id->valueint;
        cJSON_Delete(root);
        return 0;
    }

    cJSON_Delete(root);
    return -1;
}

int xz_proto_parse_mcp_request(const char *json, int *out_id, char *method, int method_sz) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return -1;

    const cJSON *payload = cJSON_GetObjectItemCaseSensitive(root, "payload");
    if (!payload) { cJSON_Delete(root); return -1; }

    const cJSON *id = cJSON_GetObjectItemCaseSensitive(payload, "id");
    if (!cJSON_IsNumber(id)) { cJSON_Delete(root); return -1; }
    *out_id = id->valueint;

    const cJSON *m = cJSON_GetObjectItemCaseSensitive(payload, "method");
    if (cJSON_IsString(m) && m->valuestring) {
        strncpy(method, m->valuestring, method_sz - 1);
        method[method_sz - 1] = '\0';
    } else {
        method[0] = '\0';
    }

    cJSON_Delete(root);
    return 0;
}

int xz_proto_build_mcp_init_result(char *buf, int bufsize, const char *session_id,
                                    int jsonrpc_id) {
    int len = snprintf(buf, bufsize,
        "{\"type\":\"mcp\",\"session_id\":\"%s\","
        "\"payload\":{\"jsonrpc\":\"2.0\",\"id\":%d,"
        "\"result\":{\"protocolVersion\":\"2024-11-05\","
        "\"capabilities\":{},"
        "\"serverInfo\":{\"name\":\"xiaozhi-r328\",\"version\":\"1.0.0\"}}}}",
        session_id, jsonrpc_id);
    return (len >= bufsize) ? -1 : len;
}

int xz_proto_build_mcp_initialized(char *buf, int bufsize, const char *session_id) {
    int len = snprintf(buf, bufsize,
        "{\"type\":\"mcp\",\"session_id\":\"%s\","
        "\"payload\":{\"jsonrpc\":\"2.0\","
        "\"method\":\"notifications/initialized\"}}",
        session_id);
    return (len >= bufsize) ? -1 : len;
}

int xz_proto_build_mcp_tools_result(char *buf, int bufsize, const char *session_id,
                                     int jsonrpc_id) {
    int len = snprintf(buf, bufsize,
        "{\"type\":\"mcp\",\"session_id\":\"%s\","
        "\"payload\":{\"jsonrpc\":\"2.0\",\"id\":%d,"
        "\"result\":{\"tools\":[]}}}",
        session_id, jsonrpc_id);
    return (len >= bufsize) ? -1 : len;
}
