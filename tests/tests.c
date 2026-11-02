#include "test_support.h"

#include <stdlib.h>
#include <stdint.h>

static aparse_status run_null_args(test_result* result)
{
    char* argv[] = {(char[]){"prog"}};
    return test_parse(result, 1, argv, NULL);
}

static aparse_status run_missing_positional(test_result* result)
{
    char value[8] = {0};
    aparse_arg args[] = {
        aparse_arg_string("value", value, sizeof(value), NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}};
    return test_parse(result, 1, argv, args);
}

static aparse_status run_invalid_subcommand(test_result* result)
{
    aparse_arg commands[] = {
        aparse_arg_subparser_impl("copy", NULL, NULL, NULL, 0, NULL, NULL, 0),
        aparse_arg_end_marker
    };
    aparse_arg args[] = {
        aparse_arg_parser("command", commands),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"invalid"}};
    return test_parse(result, 2, argv, args);
}

static aparse_status run_signed_min(test_result* result)
{
    int64_t value = 0;
    aparse_arg args[] = {
        aparse_arg_number("value", &value, sizeof(value), APARSE_ARG_TYPE_SIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"-9223372036854775808"}};
    aparse_status status = test_parse(result, 2, argv, args);
    test_expect(result, value == INT64_MIN, "signed minimum was not stored");
    return status;
}

static aparse_status run_signed_max(test_result* result)
{
    int64_t value = 0;
    aparse_arg args[] = {
        aparse_arg_number("value", &value, sizeof(value), APARSE_ARG_TYPE_SIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"9223372036854775807"}};
    aparse_status status = test_parse(result, 2, argv, args);
    test_expect(result, value == INT64_MAX, "signed maximum was not stored");
    return status;
}

static aparse_status run_signed_overflow(test_result* result)
{
    int64_t value = 0;
    aparse_arg args[] = {
        aparse_arg_number("value", &value, sizeof(value), APARSE_ARG_TYPE_SIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"9223372036854775808"}};
    return test_parse(result, 2, argv, args);
}

static aparse_status run_signed_underflow(test_result* result)
{
    int64_t value = 0;
    aparse_arg args[] = {
        aparse_arg_number("value", &value, sizeof(value), APARSE_ARG_TYPE_SIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"-9223372036854775809"}};
    return test_parse(result, 2, argv, args);
}

static aparse_status run_unsigned_max(test_result* result)
{
    uint64_t value = 0;
    aparse_arg args[] = {
        aparse_arg_number("value", &value, sizeof(value), APARSE_ARG_TYPE_UNSIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"18446744073709551615"}};
    aparse_status status = test_parse(result, 2, argv, args);
    test_expect(result, value == UINT64_MAX, "unsigned maximum was not stored");
    return status;
}

static aparse_status run_unsigned_overflow(test_result* result)
{
    uint64_t value = 0;
    aparse_arg args[] = {
        aparse_arg_number("value", &value, sizeof(value), APARSE_ARG_TYPE_UNSIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"18446744073709551616"}};
    return test_parse(result, 2, argv, args);
}

static aparse_status run_unsigned_negative(test_result* result)
{
    uint64_t value = 0;
    aparse_arg args[] = {
        aparse_arg_number("value", &value, sizeof(value), APARSE_ARG_TYPE_UNSIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"-1"}};
    return test_parse(result, 2, argv, args);
}

static aparse_status run_unsigned_leading_plus(test_result* result)
{
    uint64_t value = 0;
    aparse_arg args[] = {
        aparse_arg_number("value", &value, sizeof(value), APARSE_ARG_TYPE_UNSIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"+42"}};
    aparse_status status = test_parse(result, 2, argv, args);
    test_expect(result, value == 42, "leading plus was not parsed");
    return status;
}

static aparse_status run_unsigned_hex(test_result* result)
{
    uint32_t value = 0;
    aparse_arg args[] = {
        aparse_arg_number("value", &value, sizeof(value), APARSE_ARG_TYPE_UNSIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"0x2a"}};
    aparse_status status = test_parse(result, 2, argv, args);
    test_expect(result, value == 42, "hexadecimal value was not parsed");
    return status;
}

static aparse_status run_unsigned_octal(test_result* result)
{
    uint32_t value = 0;
    aparse_arg args[] = {
        aparse_arg_number("value", &value, sizeof(value), APARSE_ARG_TYPE_UNSIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"052"}};
    aparse_status status = test_parse(result, 2, argv, args);
    test_expect(result, value == 42, "octal value was not parsed");
    return status;
}

static aparse_status run_unsigned_binary(test_result* result)
{
    uint32_t value = 0;
    aparse_arg args[] = {
        aparse_arg_number("value", &value, sizeof(value), APARSE_ARG_TYPE_UNSIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"0b101010"}};
    aparse_status status = test_parse(result, 2, argv, args);
    test_expect(result, value == 42, "binary value was not parsed");
    return status;
}

static aparse_status run_string_copy(test_result* result)
{
    char value[16] = {0};
    aparse_arg args[] = {
        aparse_arg_string("value", value, sizeof(value), NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"hello"}};
    aparse_status status = test_parse(result, 2, argv, args);
    test_expect(result, strcmp(value, "hello") == 0, "string value mismatch");
    return status;
}

static aparse_status run_string_truncation(test_result* result)
{
    char value[5] = {0};
    aparse_arg args[] = {
        aparse_arg_string("value", value, sizeof(value), NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"abcdef"}};
    aparse_status status = test_parse(result, 2, argv, args);
    test_expect(result, strcmp(value, "abcd") == 0,
            "string was not safely truncated and terminated");
    return status;
}

static aparse_status run_string_pointer(test_result* result)
{
    const char* value = NULL;
    aparse_arg args[] = {
        aparse_arg_string("value", &value, 0, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"borrowed"}};
    aparse_status status = test_parse(result, 2, argv, args);
    test_expect(result, value == argv[1], "zero-size string did not retain argv pointer");
    return status;
}

static aparse_status run_positional_array_leaves_final(test_result* result)
{
    aparse_list values = {0};
    char final[16] = {0};
    aparse_arg args[] = {
        aparse_arg_array("values", &values, sizeof(values), 0,
                APARSE_ARG_TYPE_STRING, 0, NULL),
        aparse_arg_string("final", final, sizeof(final), NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {
        (char[]){"prog"}, (char[]){"first"}, (char[]){"second"},
        (char[]){"last"}
    };
    aparse_status status = test_parse(result, 4, argv, args);
    test_expect(result, values.size == 2, "array did not leave the final positional value");
    test_expect(result, values.size == 2 &&
            strcmp(aparse_list_get(&values, char*, 0), "first") == 0 &&
            strcmp(aparse_list_get(&values, char*, 1), "second") == 0,
            "array values were not parsed in order");
    test_expect(result, strcmp(final, "last") == 0,
            "final positional value was not parsed");
    free(values.ptr);
    return status;
}

static aparse_status run_option_equals(test_result* result)
{
    int value = 0;
    aparse_arg args[] = {
        aparse_arg_option("-n", "--number", &value, sizeof(value), APARSE_ARG_TYPE_SIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"--number=17"}};
    aparse_status status = test_parse(result, 2, argv, args);
    test_expect(result, value == 17, "option=value form was not parsed");
    return status;
}

static aparse_status run_option_separate_value(test_result* result)
{
    int value = 0;
    aparse_arg args[] = {
        aparse_arg_option("-n", "--number", &value, sizeof(value), APARSE_ARG_TYPE_SIGNED, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"-n"}, (char[]){"23"}};
    aparse_status status = test_parse(result, 3, argv, args);
    test_expect(result, value == 23, "separate option value was not parsed");
    return status;
}

static aparse_status run_bool_option(test_result* result)
{
    bool value = false;
    aparse_arg args[] = {
        aparse_arg_option("-v", "--verbose", &value, sizeof(value), APARSE_ARG_TYPE_BOOL, NULL),
        aparse_arg_end_marker
    };
    char* argv[] = {(char[]){"prog"}, (char[]){"--verbose"}};
    aparse_status status = test_parse(result, 2, argv, args);
    test_expect(result, value, "boolean option did not become true");
    return status;
}

static const test_case tests[] = {
    {"null-args", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_null_args},
    {"missing-positional", APARSE_STATUS_MISSING_POSITIONAL, APARSE_STATUS_MISSING_POSITIONAL, 1, run_missing_positional},
    {"invalid-subcommand", APARSE_STATUS_INVALID_SUBCOMMAND, APARSE_STATUS_INVALID_SUBCOMMAND, 1, run_invalid_subcommand},
    {"signed-min", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_signed_min},
    {"signed-max", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_signed_max},
    {"signed-overflow", APARSE_STATUS_OVERFLOW, APARSE_STATUS_OVERFLOW, 1, run_signed_overflow},
    {"signed-underflow", APARSE_STATUS_UNDERFLOW, APARSE_STATUS_UNDERFLOW, 1, run_signed_underflow},
    {"unsigned-max", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_unsigned_max},
    {"unsigned-overflow", APARSE_STATUS_OVERFLOW, APARSE_STATUS_OVERFLOW, 1, run_unsigned_overflow},
    {"unsigned-negative", APARSE_STATUS_UNDERFLOW, APARSE_STATUS_UNDERFLOW, 1, run_unsigned_negative},
    {"unsigned-leading-plus", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_unsigned_leading_plus},
    {"unsigned-hex", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_unsigned_hex},
    {"unsigned-octal", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_unsigned_octal},
    {"unsigned-binary", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_unsigned_binary},
    {"string-copy", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_string_copy},
    {"string-truncation", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_string_truncation},
    {"string-pointer", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_string_pointer},
    {"positional-array-leaves-final", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_positional_array_leaves_final},
    {"option-equals", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_option_equals},
    {"option-separate-value", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_option_separate_value},
    {"bool-option", APARSE_STATUS_OK, APARSE_STATUS_OK, 0, run_bool_option}
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
