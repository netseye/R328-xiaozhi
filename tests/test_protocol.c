#include "test_runner.h"
#include "protocol.h"

TEST(build_hello) {
    char buf[512];
    int len = xz_proto_build_hello(buf, sizeof(buf));
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strstr(buf, "\"type\":\"hello\"") != NULL);
    ASSERT_TRUE(strstr(buf, "\"version\":1") != NULL);
    ASSERT_TRUE(strstr(buf, "\"transport\":\"websocket\"") != NULL);
    ASSERT_TRUE(strstr(buf, "\"format\":\"opus\"") != NULL);
    ASSERT_TRUE(strstr(buf, "\"sample_rate\":16000") != NULL);
    ASSERT_TRUE(strstr(buf, "\"channels\":1") != NULL);
    ASSERT_TRUE(strstr(buf, "\"frame_duration\":60") != NULL);
}

TEST(build_listen_start) {
    char buf[256];
    int len = xz_proto_build_listen(buf, sizeof(buf), "sess123", "start", "manual");
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strstr(buf, "\"type\":\"listen\"") != NULL);
    ASSERT_TRUE(strstr(buf, "\"state\":\"start\"") != NULL);
    ASSERT_TRUE(strstr(buf, "\"mode\":\"manual\"") != NULL);
    ASSERT_TRUE(strstr(buf, "\"session_id\":\"sess123\"") != NULL);
}

TEST(build_listen_stop) {
    char buf[256];
    int len = xz_proto_build_listen(buf, sizeof(buf), "sess456", "stop", NULL);
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strstr(buf, "\"state\":\"stop\"") != NULL);
}

TEST(build_abort) {
    char buf[256];
    int len = xz_proto_build_abort(buf, sizeof(buf), "sess789", "wake_word_detected");
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strstr(buf, "\"type\":\"abort\"") != NULL);
    ASSERT_TRUE(strstr(buf, "\"reason\":\"wake_word_detected\"") != NULL);
}

TEST(parse_type_hello) {
    const char *json = "{\"type\":\"hello\",\"transport\":\"websocket\",\"session_id\":\"abc\"}";
    xz_msg_type_t type = xz_proto_parse_type(json);
    ASSERT_EQ_INT(type, XZ_MSG_HELLO);
}

TEST(parse_type_stt) {
    const char *json = "{\"type\":\"stt\",\"text\":\"hello world\"}";
    xz_msg_type_t type = xz_proto_parse_type(json);
    ASSERT_EQ_INT(type, XZ_MSG_STT);
}

TEST(parse_type_tts_start) {
    const char *json = "{\"type\":\"tts\",\"state\":\"start\"}";
    xz_msg_type_t type = xz_proto_parse_type(json);
    ASSERT_EQ_INT(type, XZ_MSG_TTS_START);
}

TEST(parse_type_tts_stop) {
    const char *json = "{\"type\":\"tts\",\"state\":\"stop\"}";
    xz_msg_type_t type = xz_proto_parse_type(json);
    ASSERT_EQ_INT(type, XZ_MSG_TTS_STOP);
}

TEST(parse_type_llm) {
    const char *json = "{\"type\":\"llm\",\"emotion\":\"happy\",\"text\":\":)\"}";
    xz_msg_type_t type = xz_proto_parse_type(json);
    ASSERT_EQ_INT(type, XZ_MSG_LLM);
}

TEST(parse_type_unknown) {
    const char *json = "{\"type\":\"something_else\"}";
    xz_msg_type_t type = xz_proto_parse_type(json);
    ASSERT_EQ_INT(type, XZ_MSG_UNKNOWN);
}

TEST(parse_type_invalid) {
    xz_msg_type_t type = xz_proto_parse_type("not json");
    ASSERT_EQ_INT(type, XZ_MSG_UNKNOWN);
}

TEST(parse_hello_ack) {
    const char *json =
        "{\"type\":\"hello\",\"transport\":\"websocket\","
        "\"session_id\":\"sess-abc-123\","
        "\"audio_params\":{\"format\":\"opus\",\"sample_rate\":24000}}";
    xz_hello_ack_t ack;
    int rc = xz_proto_parse_hello_ack(json, &ack);
    ASSERT_EQ_INT(rc, 0);
    ASSERT_EQ_STR(ack.session_id, "sess-abc-123");
    ASSERT_EQ_INT(ack.audio_sample_rate, 24000);
}

TEST(parse_stt) {
    const char *json = "{\"type\":\"stt\",\"text\":\"hello world\",\"session_id\":\"s1\"}";
    xz_stt_t stt;
    int rc = xz_proto_parse_stt(json, &stt);
    ASSERT_EQ_INT(rc, 0);
    ASSERT_EQ_STR(stt.text, "hello world");
}

TEST(buffer_too_small) {
    char buf[10];
    int len = xz_proto_build_hello(buf, sizeof(buf));
    ASSERT_EQ_INT(len, -1);
}

static void run_tests(void) {
    RUN_TEST(build_hello);
    RUN_TEST(build_listen_start);
    RUN_TEST(build_listen_stop);
    RUN_TEST(build_abort);
    RUN_TEST(parse_type_hello);
    RUN_TEST(parse_type_stt);
    RUN_TEST(parse_type_tts_start);
    RUN_TEST(parse_type_tts_stop);
    RUN_TEST(parse_type_llm);
    RUN_TEST(parse_type_unknown);
    RUN_TEST(parse_type_invalid);
    RUN_TEST(parse_hello_ack);
    RUN_TEST(parse_stt);
    RUN_TEST(buffer_too_small);
}

TEST_MAIN()
