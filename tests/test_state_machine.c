#include "test_runner.h"
#include "state_machine.h"

static int last_action = 0;

static void on_action(xz_state_t from, xz_state_t to, void *user_data) {
    (void)user_data;
    last_action = from * 10 + to;
}

TEST(initial_state_is_idle) {
    xz_sm_t sm;
    xz_sm_init(&sm, on_action, NULL);
    ASSERT_EQ_INT(xz_sm_current(&sm), XZ_STATE_IDLE);
}

TEST(idle_to_connecting_on_trigger) {
    xz_sm_t sm;
    xz_sm_init(&sm, on_action, NULL);
    int rc = xz_sm_event(&sm, XZ_EVENT_USER_TRIGGER);
    ASSERT_EQ_INT(rc, 0);
    ASSERT_EQ_INT(xz_sm_current(&sm), XZ_STATE_CONNECTING);
    ASSERT_EQ_INT(last_action, XZ_STATE_IDLE * 10 + XZ_STATE_CONNECTING);
}

TEST(connecting_to_listening_on_hello_ack) {
    xz_sm_t sm;
    xz_sm_init(&sm, on_action, NULL);
    xz_sm_event(&sm, XZ_EVENT_USER_TRIGGER);
    int rc = xz_sm_event(&sm, XZ_EVENT_HELLO_ACK);
    ASSERT_EQ_INT(rc, 0);
    ASSERT_EQ_INT(xz_sm_current(&sm), XZ_STATE_LISTENING);
}

TEST(listening_to_speaking_on_tts_start) {
    xz_sm_t sm;
    xz_sm_init(&sm, on_action, NULL);
    xz_sm_event(&sm, XZ_EVENT_USER_TRIGGER);
    xz_sm_event(&sm, XZ_EVENT_HELLO_ACK);
    int rc = xz_sm_event(&sm, XZ_EVENT_TTS_START);
    ASSERT_EQ_INT(rc, 0);
    ASSERT_EQ_INT(xz_sm_current(&sm), XZ_STATE_SPEAKING);
}

TEST(speaking_to_idle_on_tts_stop_manual) {
    xz_sm_t sm;
    xz_sm_init(&sm, on_action, NULL);
    sm.mode = XZ_MODE_MANUAL;
    xz_sm_event(&sm, XZ_EVENT_USER_TRIGGER);
    xz_sm_event(&sm, XZ_EVENT_HELLO_ACK);
    xz_sm_event(&sm, XZ_EVENT_TTS_START);
    int rc = xz_sm_event(&sm, XZ_EVENT_TTS_STOP);
    ASSERT_EQ_INT(rc, 0);
    ASSERT_EQ_INT(xz_sm_current(&sm), XZ_STATE_IDLE);
}

TEST(speaking_to_listening_on_tts_stop_auto) {
    xz_sm_t sm;
    xz_sm_init(&sm, on_action, NULL);
    sm.mode = XZ_MODE_AUTO;
    xz_sm_event(&sm, XZ_EVENT_USER_TRIGGER);
    xz_sm_event(&sm, XZ_EVENT_HELLO_ACK);
    xz_sm_event(&sm, XZ_EVENT_TTS_START);
    int rc = xz_sm_event(&sm, XZ_EVENT_TTS_STOP);
    ASSERT_EQ_INT(rc, 0);
    ASSERT_EQ_INT(xz_sm_current(&sm), XZ_STATE_LISTENING);
}

TEST(disconnect_returns_to_idle) {
    xz_sm_t sm;
    xz_sm_init(&sm, on_action, NULL);
    xz_sm_event(&sm, XZ_EVENT_USER_TRIGGER);
    xz_sm_event(&sm, XZ_EVENT_HELLO_ACK);
    xz_sm_event(&sm, XZ_EVENT_TTS_START);
    int rc = xz_sm_event(&sm, XZ_EVENT_DISCONNECTED);
    ASSERT_EQ_INT(rc, 0);
    ASSERT_EQ_INT(xz_sm_current(&sm), XZ_STATE_IDLE);
}

TEST(invalid_transition_rejected) {
    xz_sm_t sm;
    xz_sm_init(&sm, on_action, NULL);
    int rc = xz_sm_event(&sm, XZ_EVENT_TTS_START);
    ASSERT_EQ_INT(rc, -1);
    ASSERT_EQ_INT(xz_sm_current(&sm), XZ_STATE_IDLE);
}

TEST(abort_from_listening) {
    xz_sm_t sm;
    xz_sm_init(&sm, on_action, NULL);
    xz_sm_event(&sm, XZ_EVENT_USER_TRIGGER);
    xz_sm_event(&sm, XZ_EVENT_HELLO_ACK);
    int rc = xz_sm_event(&sm, XZ_EVENT_ABORT);
    ASSERT_EQ_INT(rc, 0);
    ASSERT_EQ_INT(xz_sm_current(&sm), XZ_STATE_IDLE);
}

static void run_tests(void) {
    RUN_TEST(initial_state_is_idle);
    RUN_TEST(idle_to_connecting_on_trigger);
    RUN_TEST(connecting_to_listening_on_hello_ack);
    RUN_TEST(listening_to_speaking_on_tts_start);
    RUN_TEST(speaking_to_idle_on_tts_stop_manual);
    RUN_TEST(speaking_to_listening_on_tts_stop_auto);
    RUN_TEST(disconnect_returns_to_idle);
    RUN_TEST(invalid_transition_rejected);
    RUN_TEST(abort_from_listening);
}

TEST_MAIN()
