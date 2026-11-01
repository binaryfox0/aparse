#include "test_support.h"

#include <stdbool.h>

static aparse_status run_missing_option_value(test_result* result)
{
    int value = 0;
    aparse_arg args[] = {
        aparse_arg_option("-n", "--count", &value, sizeof(value), APARSE_ARG_TYPE_SIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"--count"}};
    return test_parse(result, 2, argv, args);
}

static aparse_status run_invalid_option_value(test_result* result)
{
    int value = 0;
    aparse_arg args[] = {
        aparse_arg_option("-n", "--count", &value, sizeof(value), APARSE_ARG_TYPE_SIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"--count"}, (char[]){"abc"}};
    return test_parse(result, 3, argv, args);
}

static aparse_status run_repeated_boolean(test_result* result)
{
    bool value = false;
    aparse_arg args[] = {
        aparse_arg_option("-v", "--verbose", &value, sizeof(value), APARSE_ARG_TYPE_BOOL, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"--verbose"}, (char[]){"--verbose"}};
    return test_parse(result, 3, argv, args);
}

static aparse_status run_unknown_option(test_result* result)
{
    aparse_arg args[] = {aparse_arg_end_marker};
    char* argv[] = {(char[]){"prog"}, (char[]){"--unknown"}};
    return test_parse(result, 2, argv, args);
}

static aparse_status run_null_destination(test_result* result)
{
    aparse_arg args[] = {
        aparse_arg_number("value", NULL, sizeof(int), APARSE_ARG_TYPE_SIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"42"}};
    return test_parse(result, 2, argv, args);
}

static aparse_status run_invalid_numeric_size(test_result* result)
{
    int value = 0;
    aparse_arg args[] = {
        aparse_arg_number("value", &value, 0, APARSE_ARG_TYPE_SIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"42"}};
    return test_parse(result, 2, argv, args);
}

static aparse_status run_float_overflow(test_result* result)
{
    double value = 0.0;
    aparse_arg args[] = {
        aparse_arg_option("-f", "--float", &value, sizeof(value), APARSE_ARG_TYPE_FLOAT, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"--float"}, (char[]){"1e99999"}};
    return test_parse(result, 3, argv, args);
}

static aparse_status run_float_invalid(test_result* result)
{
    double value = 0.0;
    aparse_arg args[] = {
        aparse_arg_option("-f", "--float", &value, sizeof(value), APARSE_ARG_TYPE_FLOAT, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"--float=not-a-number"}};
    return test_parse(result, 2, argv, args);
}

static const test_case tests[] = {
    {"missing-option-value", APARSE_STATUS_MISSING_VALUE, APARSE_STATUS_MISSING_VALUE, 1, run_missing_option_value},
    {"invalid-option-value", APARSE_STATUS_INVALID_VALUE, APARSE_STATUS_INVALID_VALUE, 1, run_invalid_option_value},
    {"repeated-boolean", APARSE_STATUS_FAILURE, APARSE_STATUS_OK, 0, run_repeated_boolean},
    {"unknown-option", APARSE_STATUS_FAILURE, APARSE_STATUS_UNKNOWN_ARGUMENT, 1, run_unknown_option},
    {"null-destination", APARSE_STATUS_OK, APARSE_STATUS_NULL_POINTER, 1, run_null_destination},
    {"invalid-numeric-size", APARSE_STATUS_INVALID_SIZE, APARSE_STATUS_INVALID_SIZE, 1, run_invalid_numeric_size},
    {"float-overflow", APARSE_STATUS_OVERFLOW, APARSE_STATUS_OVERFLOW, 1, run_float_overflow},
    {"float-invalid", APARSE_STATUS_INVALID_VALUE, APARSE_STATUS_INVALID_VALUE, 1, run_float_invalid}
};

int main(int argc, char** argv)
{
    const char* filter = argc > 1 ? argv[1] : NULL;
    if (argc > 2) {
        fprintf(stderr, "usage: %s [all|test-name]\n", argv[0]);
        return 2;
    }
    return run_test_cases(tests, sizeof(tests) / sizeof(tests[0]), filter);
}
