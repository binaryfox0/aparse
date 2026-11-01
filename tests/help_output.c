#include "test_support.h"

#include <stdbool.h>

static aparse_status run_long_help(test_result* result)
{
    bool verbose = false;
    int count = 0;
    aparse_arg args[] = {
        aparse_arg_option("-v", "--verbose", &verbose, sizeof(verbose), APARSE_ARG_TYPE_BOOL, "Toggle verbosity"),
        aparse_arg_option("-n", "--count", &count, sizeof(count), APARSE_ARG_TYPE_SIGNED, "Number of items"),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"--help"}};
    return test_parse(result, 2, argv, args);
}

static aparse_status run_short_help(test_result* result)
{
    bool verbose = false;
    aparse_arg args[] = {
        aparse_arg_option("-v", "--verbose", &verbose, sizeof(verbose), APARSE_ARG_TYPE_BOOL, "Toggle verbosity"),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"-h"}};
    return test_parse(result, 2, argv, args);
}

static const test_case tests[] = {
    {"long-help", APARSE_STATUS_FAILURE, APARSE_STATUS_OK, 0, run_long_help},
    {"short-help", APARSE_STATUS_FAILURE, APARSE_STATUS_OK, 0, run_short_help}
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
