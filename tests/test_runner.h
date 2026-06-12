#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    tests_run++; \
    printf("  %-50s", #name); \
    test_##name(); \
    tests_passed++; \
    printf("PASS\n"); \
} while (0)

#define ASSERT_EQ_INT(a, b) do { \
    if ((a) != (b)) { \
        printf("FAIL\n    Expected %d, got %d (%s:%d)\n", (int)(b), (int)(a), __FILE__, __LINE__); \
        tests_failed++; \
        tests_run++; \
        return; \
    } \
} while (0)

#define ASSERT_EQ_STR(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("FAIL\n    Expected \"%s\", got \"%s\" (%s:%d)\n", (b), (a), __FILE__, __LINE__); \
        tests_failed++; \
        tests_run++; \
        return; \
    } \
} while (0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        tests_failed++; \
        tests_run++; \
        return; \
    } \
} while (0)

#define ASSERT_NULL(p) ASSERT_TRUE((p) == NULL)
#define ASSERT_NOT_NULL(p) ASSERT_TRUE((p) != NULL)

#define TEST_MAIN() int main(void) { \
    printf("Running tests...\n\n"); \
    run_tests(); \
    printf("\n%d tests, %d passed, %d failed\n", tests_run, tests_passed, tests_failed); \
    return tests_failed > 0 ? 1 : 0; \
}

#endif
