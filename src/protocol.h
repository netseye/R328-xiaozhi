#ifndef XZ_PROTOCOL_H
#define XZ_PROTOCOL_H

#include <stddef.h>

#define XZ_MAX_SESSION 128
#define XZ_MAX_TEXT 1024

typedef enum {
    XZ_MSG_UNKNOWN = 0,
    XZ_MSG_HELLO,
    XZ_MSG_STT,
    XZ_MSG_TTS_START,
    XZ_MSG_TTS_STOP,
    XZ_MSG_TTS_SENTENCE,
    XZ_MSG_LLM,
    XZ_MSG_MCP,
    XZ_MSG_SYSTEM,
    XZ_MSG_ALERT,
    XZ_MSG_LISTEN,
    XZ_MSG_ABORT
} xz_msg_type_t;

typedef struct {
    char session_id[XZ_MAX_SESSION];
    int  audio_sample_rate;
    char audio_format[32];
} xz_hello_ack_t;

typedef struct {
    char session_id[XZ_MAX_SESSION];
    char text[XZ_MAX_TEXT];
} xz_stt_t;

/* Build messages (returns bytes written, -1 on buffer overflow) */
int xz_proto_build_hello(char *buf, int bufsize);
int xz_proto_build_listen(char *buf, int bufsize, const char *session_id,
                           const char *state, const char *mode);
int xz_proto_build_abort(char *buf, int bufsize, const char *session_id,
                          const char *reason);

/* Parse incoming messages */
xz_msg_type_t xz_proto_parse_type(const char *json);
int xz_proto_parse_hello_ack(const char *json, xz_hello_ack_t *ack);
int xz_proto_parse_stt(const char *json, xz_stt_t *stt);

#endif
