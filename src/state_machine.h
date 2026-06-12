#ifndef XZ_STATE_MACHINE_H
#define XZ_STATE_MACHINE_H

typedef enum {
    XZ_STATE_IDLE = 1,
    XZ_STATE_CONNECTING = 2,
    XZ_STATE_LISTENING = 3,
    XZ_STATE_SPEAKING = 4
} xz_state_t;

typedef enum {
    XZ_EVENT_USER_TRIGGER = 1,
    XZ_EVENT_HELLO_ACK = 2,
    XZ_EVENT_TTS_START = 3,
    XZ_EVENT_TTS_STOP = 4,
    XZ_EVENT_DISCONNECTED = 5,
    XZ_EVENT_ABORT = 6
} xz_event_t;

typedef enum {
    XZ_MODE_MANUAL = 0,
    XZ_MODE_AUTO = 1
} xz_mode_t;

typedef struct xz_sm_t xz_sm_t;

typedef void (*xz_transition_cb)(xz_state_t from, xz_state_t to, void *user_data);

struct xz_sm_t {
    xz_state_t current;
    xz_mode_t  mode;
    xz_transition_cb on_transition;
    void *user_data;
};

void xz_sm_init(xz_sm_t *sm, xz_transition_cb cb, void *user_data);
xz_state_t xz_sm_current(const xz_sm_t *sm);
int xz_sm_event(xz_sm_t *sm, xz_event_t event);

#endif
