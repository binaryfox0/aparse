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

static aparse_status run_subparser_without_layout(test_result* result)
{
    aparse_arg arg = aparse_arg_subparser(
            "command", NULL, NULL, NULL, 0, "Command without payload layout");
    test_expect(result, arg.layout_size == 0,
            "subparser without layout should have zero layout size");
    return APARSE_STATUS_OK;
}

static void run_dispatch_handler(void* data)
{
    (void)data;
}

static aparse_status run_dispatch_all_releases_storage(test_result* result)
{
    aparse_arg commands[] = {
        aparse_subparser("run", NULL, run_dispatch_handler, NULL),
        aparse_arg_end_marker
    };
    aparse_arg args[] = {
        aparse_arg_parser("command", commands),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"run"}};
    aparse_list dispatch = {0};
    aparse_status status = aparse_parse(2, argv, args, &dispatch, NULL);

    test_expect(result, dispatch.ptr != NULL && dispatch.size == 1,
            "successful parse should return one queued dispatch");
    aparse_dispatch_all(&dispatch);
    test_expect(result, dispatch.ptr == NULL && dispatch.size == 0 &&
            dispatch.capacity == 0,
            "dispatch_all should release the dispatch list storage");
    return status;
}

static aparse_status run_dispatch_free_releases_storage(test_result* result)
{
    aparse_arg commands[] = {
        aparse_subparser("run", NULL, run_dispatch_handler, NULL),
        aparse_arg_end_marker
    };
    aparse_arg args[] = {
        aparse_arg_parser("command", commands),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"run"}};
    aparse_list dispatch = {0};
    aparse_status status = aparse_parse(2, argv, args, &dispatch, NULL);

    aparse_dispatch_free(&dispatch);
    test_expect(result, dispatch.ptr == NULL && dispatch.size == 0 &&
            dispatch.capacity == 0,
            "dispatch_free should release the dispatch list storage");
    return status;
}

static aparse_status run_dispatch_preserves_caller_owned_array(test_result* result)
{
    typedef struct {
        aparse_list placeholder;
    } test_payload;

    aparse_list values = {0};
    size_t layout[] = {offsetof(test_payload, placeholder), sizeof(aparse_list)};
    aparse_arg command_args[] = {
        aparse_arg_array("values", &values, sizeof(values), 0,
                APARSE_ARG_TYPE_STRING, 0, NULL),
        aparse_arg_end_marker
    };
    aparse_arg commands[] = {
        aparse_arg_subparser_impl("run", command_args, run_dispatch_handler,
                NULL, 0, NULL, layout, 1),
        aparse_arg_end_marker
    };
    aparse_arg args[] = {
        aparse_arg_parser("command", commands),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"run"},
        (char[]){"first"}, (char[]){"second"}};
    aparse_list dispatch = {0};
    aparse_status status = aparse_parse(4, argv, args, &dispatch, NULL);

    aparse_dispatch_all(&dispatch);
    test_expect(result, values.ptr != NULL && values.size == 2,
            "dispatch should preserve arrays stored in caller-owned destinations");
    aparse_list_free(&values);
    return status;
}

static aparse_status run_failed_parse_releases_dispatch(test_result* result)
{
    char required[8] = {0};
    aparse_arg commands[] = {
        aparse_subparser("run", NULL, run_dispatch_handler, NULL),
        aparse_arg_end_marker
    };
    aparse_arg args[] = {
        aparse_arg_parser("command", commands),
        aparse_arg_string("required", required, sizeof(required), NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"run"}};
    aparse_list dispatch = {0};

    aparse_set_error_callback(test_capture_error, result);
    aparse_status status = aparse_parse(2, argv, args, &dispatch, NULL);
    aparse_set_error_callback(NULL, NULL);

    test_expect(result, dispatch.ptr == NULL,
            "failed parse should not transfer its queued dispatch list");
    return status;
}

static const test_case tests[] = {
    {"missing-option-value", APARSE_STATUS_MISSING_VALUE, APARSE_STATUS_MISSING_VALUE, 1, run_missing_option_value},
    {"invalid-option-value", APARSE_STATUS_INVALID_VALUE, APARSE_STATUS_INVALID_VALUE, 1, run_invalid_option_value},
    {"repeated-boolean", APARSE_STATUS_FAILURE, APARSE_STATUS_OK, 0, run_repeated_boolean},
    {"unknown-option", APARSE_STATUS_FAILURE, APARSE_STATUS_UNKNOWN_ARGUMENT, 1, run_unknown_option},
    {"null-destination", APARSE_STATUS_OK, APARSE_STATUS_NULL_POINTER, 1, run_null_destination},
    {"invalid-numeric-size", APARSE_STATUS_INVALID_SIZE, APARSE_STATUS_INVALID_SIZE, 1, run_invalid_numeric_size},
    {"float-overflow", APARSE_STATUS_OVERFLOW, APARSE_STATUS_OVERFLOW, 1, run_float_overflow},
    {"float-invalid", APARSE_STATUS_INVALID_VALUE, APARSE_STATUS_INVALID_VALUE, 1, run_float_invalid},
    {"subparser-without-layout", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_subparser_without_layout},
    {"dispatch-all-releases-storage", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_dispatch_all_releases_storage},
    {"dispatch-free-releases-storage", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_dispatch_free_releases_storage},
    {"dispatch-preserves-caller-owned-array", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_dispatch_preserves_caller_owned_array},
    {"failed-parse-releases-dispatch", APARSE_STATUS_MISSING_POSITIONAL, APARSE_STATUS_MISSING_POSITIONAL, 1, run_failed_parse_releases_dispatch}
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
