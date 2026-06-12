#include "state_machine.h"

void xz_sm_init(xz_sm_t *sm, xz_transition_cb cb, void *user_data) {
    sm->current = XZ_STATE_IDLE;
    sm->mode = XZ_MODE_MANUAL;
    sm->on_transition = cb;
    sm->user_data = user_data;
}

xz_state_t xz_sm_current(const xz_sm_t *sm) {
    return sm->current;
}

static int transition(xz_sm_t *sm, xz_state_t next) {
    xz_state_t prev = sm->current;
    sm->current = next;
    if (sm->on_transition) {
        sm->on_transition(prev, next, sm->user_data);
    }
    return 0;
}

int xz_sm_event(xz_sm_t *sm, xz_event_t event) {
    switch (sm->current) {
    case XZ_STATE_IDLE:
        if (event == XZ_EVENT_USER_TRIGGER)
            return transition(sm, XZ_STATE_CONNECTING);
        return -1;

    case XZ_STATE_CONNECTING:
        if (event == XZ_EVENT_HELLO_ACK)
            return transition(sm, XZ_STATE_LISTENING);
        if (event == XZ_EVENT_DISCONNECTED)
            return transition(sm, XZ_STATE_IDLE);
        return -1;

    case XZ_STATE_LISTENING:
        if (event == XZ_EVENT_TTS_START)
            return transition(sm, XZ_STATE_SPEAKING);
        if (event == XZ_EVENT_DISCONNECTED || event == XZ_EVENT_ABORT)
            return transition(sm, XZ_STATE_IDLE);
        return -1;

    case XZ_STATE_SPEAKING:
        if (event == XZ_EVENT_TTS_STOP) {
            if (sm->mode == XZ_MODE_AUTO)
                return transition(sm, XZ_STATE_LISTENING);
            else
                return transition(sm, XZ_STATE_IDLE);
        }
        if (event == XZ_EVENT_DISCONNECTED || event == XZ_EVENT_ABORT)
            return transition(sm, XZ_STATE_IDLE);
        return -1;
    }

    return -1;
}
