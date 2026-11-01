#ifndef APARSE_TEST_SUPPORT_H
#define APARSE_TEST_SUPPORT_H

#include "aparse.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct test_result {
    aparse_status callback_status;
    size_t callback_count;
    bool assertion_failed;
    const char* assertion_message;
} test_result;

typedef aparse_status (*test_run_fn)(test_result* result);

typedef struct test_case {
    const char* name;
    aparse_status expected_status;
    aparse_status expected_callback_status;
    size_t expected_callback_count;
    test_run_fn run;
} test_case;

static void test_capture_error(
        const aparse_context* context,
        const aparse_status status,
        const void* field1,
        const void* field2,
        void* userdata)
{
    test_result* result = userdata;
    (void)context;
    (void)field1;
    (void)field2;
    result->callback_status = status;
    result->callback_count++;
}

static inline aparse_status test_parse(
        test_result* result,
        const int argc,
        char* const* argv,
        aparse_arg* args)
{
    result->callback_status = APARSE_STATUS_OK;
    result->callback_count = 0;
    aparse_set_error_callback(test_capture_error, result);
    aparse_status status = aparse_parse(argc, argv, args, NULL, NULL);
    aparse_set_error_callback(NULL, NULL);
    return status;
}

static inline void test_expect(
        test_result* result,
        const bool condition,
        const char* message)
{
    if (!condition && !result->assertion_failed) {
        result->assertion_failed = true;
        result->assertion_message = message;
    }
}

static inline int run_test_cases(
        const test_case* tests,
        const size_t test_count,
        const char* filter)
{
    size_t passed = 0;
    size_t failed = 0;
    size_t selected = 0;

    for (size_t i = 0; i < test_count; i++) {
        if (filter && strcmp(filter, "all") != 0 &&
                strcmp(filter, tests[i].name) != 0)
            continue;

        selected++;
        test_result result = {
            .callback_status = APARSE_STATUS_OK,
            .callback_count = 0,
            .assertion_failed = false,
            .assertion_message = NULL
        };
        aparse_status status = tests[i].run(&result);
        bool case_failed = false;

        if (status != tests[i].expected_status) {
            fprintf(stderr, "%s: expected parse status %s, got %s\n",
                    tests[i].name,
                    aparse_error_msg(tests[i].expected_status),
                    aparse_error_msg(status));
            case_failed = true;
        }
        if (result.callback_count != tests[i].expected_callback_count ||
                (result.callback_count > 0 &&
                 result.callback_status != tests[i].expected_callback_status)) {
            fprintf(stderr, "%s: unexpected callback status/count\n",
                    tests[i].name);
            case_failed = true;
        }
        if (result.assertion_failed) {
            fprintf(stderr, "%s: %s\n", tests[i].name,
                    result.assertion_message);
            case_failed = true;
        }

        if (case_failed) {
            failed++;
        } else {
            printf("%s: passed\n", tests[i].name);
            passed++;
        }
    }

    if (selected == 0) {
        fprintf(stderr, "unknown test: %s\navailable tests:\n",
                filter ? filter : "(none)");
        for (size_t i = 0; i < test_count; i++)
            fprintf(stderr, "  %s\n", tests[i].name);
        return 2;
    }

    printf("summary: %zu passed, %zu failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}

#endif
