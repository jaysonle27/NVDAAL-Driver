/**
 * @file nvdaal_test.h
 * @brief Lightweight test framework for NVDAAL
 *
 * Simple C test framework with assertions, timing, and colored output.
 * No external dependencies - works on macOS.
 */

#ifndef NVDAAL_TEST_H
#define NVDAAL_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Colors for terminal output
#define TEST_COLOR_RED     "\033[0;31m"
#define TEST_COLOR_GREEN   "\033[0;32m"
#define TEST_COLOR_YELLOW  "\033[0;33m"
#define TEST_COLOR_BLUE    "\033[0;34m"
#define TEST_COLOR_RESET   "\033[0m"

// Test result counters (global)
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;
static int g_assertions = 0;
static clock_t g_suite_start;
static const char *g_current_test = NULL;

// ============================================================================
// Test Registration and Execution
// ============================================================================

typedef void (*test_func_t)(void);

typedef struct {
    const char *name;
    test_func_t func;
} test_case_t;

#define TEST_CASE(name) { #name, name }
#define TEST_END { NULL, NULL }

// ============================================================================
// Assertion Macros
// ============================================================================

#define TEST_ASSERT(expr) do { \
    g_assertions++; \
    if (!(expr)) { \
        printf("%s    FAIL: %s:%d: %s%s\n", \
               TEST_COLOR_RED, __FILE__, __LINE__, #expr, TEST_COLOR_RESET); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_MSG(expr, msg) do { \
    g_assertions++; \
    if (!(expr)) { \
        printf("%s    FAIL: %s:%d: %s - %s%s\n", \
               TEST_COLOR_RED, __FILE__, __LINE__, #expr, msg, TEST_COLOR_RESET); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_EQ(expected, actual) do { \
    g_assertions++; \
    if ((expected) != (actual)) { \
        printf("%s    FAIL: %s:%d: expected %lld, got %lld%s\n", \
               TEST_COLOR_RED, __FILE__, __LINE__, \
               (long long)(expected), (long long)(actual), TEST_COLOR_RESET); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_NEQ(not_expected, actual) do { \
    g_assertions++; \
    if ((not_expected) == (actual)) { \
        printf("%s    FAIL: %s:%d: should not be %lld%s\n", \
               TEST_COLOR_RED, __FILE__, __LINE__, \
               (long long)(actual), TEST_COLOR_RESET); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_NULL(ptr) do { \
    g_assertions++; \
    if ((ptr) != NULL) { \
        printf("%s    FAIL: %s:%d: expected NULL%s\n", \
               TEST_COLOR_RED, __FILE__, __LINE__, TEST_COLOR_RESET); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_NOT_NULL(ptr) do { \
    g_assertions++; \
    if ((ptr) == NULL) { \
        printf("%s    FAIL: %s:%d: expected non-NULL%s\n", \
               TEST_COLOR_RED, __FILE__, __LINE__, TEST_COLOR_RESET); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_STR_EQ(expected, actual) do { \
    g_assertions++; \
    if (strcmp((expected), (actual)) != 0) { \
        printf("%s    FAIL: %s:%d: expected \"%s\", got \"%s\"%s\n", \
               TEST_COLOR_RED, __FILE__, __LINE__, \
               (expected), (actual), TEST_COLOR_RESET); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_MEM_EQ(expected, actual, size) do { \
    g_assertions++; \
    if (memcmp((expected), (actual), (size)) != 0) { \
        printf("%s    FAIL: %s:%d: memory mismatch (%zu bytes)%s\n", \
               TEST_COLOR_RED, __FILE__, __LINE__, (size_t)(size), TEST_COLOR_RESET); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_IN_RANGE(val, min, max) do { \
    g_assertions++; \
    if ((val) < (min) || (val) > (max)) { \
        printf("%s    FAIL: %s:%d: %lld not in range [%lld, %lld]%s\n", \
               TEST_COLOR_RED, __FILE__, __LINE__, \
               (long long)(val), (long long)(min), (long long)(max), TEST_COLOR_RESET); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

// Skip a test
#define TEST_SKIP(reason) do { \
    printf("%s    SKIP: %s%s\n", TEST_COLOR_YELLOW, reason, TEST_COLOR_RESET); \
    return; \
} while(0)

// ============================================================================
// Test Runner
// ============================================================================

static inline void test_suite_begin(const char *name) {
    printf("\n%s========================================%s\n", TEST_COLOR_BLUE, TEST_COLOR_RESET);
    printf("%s  %s%s\n", TEST_COLOR_BLUE, name, TEST_COLOR_RESET);
    printf("%s========================================%s\n\n", TEST_COLOR_BLUE, TEST_COLOR_RESET);
    g_tests_run = 0;
    g_tests_passed = 0;
    g_tests_failed = 0;
    g_assertions = 0;
    g_suite_start = clock();
}

static inline void test_run(const char *name, test_func_t func) {
    g_tests_run++;
    g_current_test = name;
    int failed_before = g_tests_failed;

    clock_t start = clock();
    func();
    clock_t end = clock();

    double ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;

    if (g_tests_failed == failed_before) {
        g_tests_passed++;
        printf("%s  [PASS]%s %s (%.2fms)\n",
               TEST_COLOR_GREEN, TEST_COLOR_RESET, name, ms);
    } else {
        printf("%s  [FAIL]%s %s (%.2fms)\n",
               TEST_COLOR_RED, TEST_COLOR_RESET, name, ms);
    }
}

static inline int test_suite_end(void) {
    clock_t end = clock();
    double total_ms = ((double)(end - g_suite_start) / CLOCKS_PER_SEC) * 1000.0;

    printf("\n%s----------------------------------------%s\n", TEST_COLOR_BLUE, TEST_COLOR_RESET);
    printf("  Tests:      %d passed, %d failed, %d total\n",
           g_tests_passed, g_tests_failed, g_tests_run);
    printf("  Assertions: %d\n", g_assertions);
    printf("  Time:       %.2fms\n", total_ms);
    printf("%s----------------------------------------%s\n", TEST_COLOR_BLUE, TEST_COLOR_RESET);

    if (g_tests_failed == 0) {
        printf("%s  ALL TESTS PASSED%s\n\n", TEST_COLOR_GREEN, TEST_COLOR_RESET);
        return 0;
    } else {
        printf("%s  %d TEST(S) FAILED%s\n\n", TEST_COLOR_RED, g_tests_failed, TEST_COLOR_RESET);
        return 1;
    }
}

// Run all tests from a test_case_t array
static inline int test_run_all(const char *suite_name, test_case_t *tests) {
    test_suite_begin(suite_name);

    for (int i = 0; tests[i].name != NULL; i++) {
        test_run(tests[i].name, tests[i].func);
    }

    return test_suite_end();
}

// ============================================================================
// Utility Macros
// ============================================================================

// Create a test main function
#define TEST_MAIN(suite_name, ...) \
    int main(int argc, char *argv[]) { \
        (void)argc; (void)argv; \
        test_case_t tests[] = { __VA_ARGS__, TEST_END }; \
        return test_run_all(suite_name, tests); \
    }

#endif // NVDAAL_TEST_H
