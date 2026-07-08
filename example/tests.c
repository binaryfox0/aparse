#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "aparse.h"

#ifndef APARSE_PLATFORM_WIN32
#   include <unistd.h>
#   include <limits.h>
#   include <spawn.h>
#   include <sys/wait.h>
#else
#endif

#define error aparse_prog_error
#define info aparse_prog_info

#define ARRSZ(arr) (sizeof((arr)) / sizeof((arr)[0]))

#define BUFFER_SIZE 512
#define BUFFER_ZEROED_HASH 0x4D7705C5

typedef struct test_entry {
    const char* name;
    int argc;
    const char** argv;
    aparse_arg* args;
    aparse_status expected;
    uint32_t hash;
} test_entry;


static inline uint32_t fnv1a(
        const uint8_t *data,
        const size_t size)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < size; i++) 
    {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

static const char* status_string(const aparse_status status)
{
    const char* strings[__APARSE_STATUS_ENUM_END__] = {
        [APARSE_STATUS_OK]                  = "APARSE_STATUS_OK",
        [APARSE_STATUS_FAILURE]             = "APARSE_STATUS_FAILURE",
        [APARSE_STATUS_UNKNOWN_ARGUMENT]    = "APARSE_STATUS_UNKNOWN_ARGUMENT",
        [APARSE_STATUS_INVALID_VALUE]       = "APARSE_STATUS_INVALID_VALUE",
        [APARSE_STATUS_OVERFLOW]            = "APARSE_STATUS_OVERFLOW",
        [APARSE_STATUS_UNDERFLOW]           = "APARSE_STATUS_UNDERFLOW",
        [APARSE_STATUS_MISSING_POSITIONAL]  = "APARSE_STATUS_MISSING_POSITIONAL",
        [APARSE_STATUS_INVALID_SUBCOMMAND]  = "APARSE_STATUS_INVALID_SUBCOMMAND",
        [APARSE_STATUS_NULL_POINTER]        = "APARSE_STATUS_NULL_POINTER",
        [APARSE_STATUS_INVALID_TYPE]        = "APARSE_STATUS_INVALID_TYPE",
        [APARSE_STATUS_INVALID_SIZE]        = "APARSE_STATUS_INVALID_SIZE",
        [APARSE_STATUS_ALLOC_FAILURE]       = "APARSE_STATUS_ALLOC_FAILURE",
        [APARSE_STATUS_UNHANDLED]           = "APARSE_STATUS_UNHANDLED",
    };
    if(status < 0 || status >= __APARSE_STATUS_ENUM_END__)
        return 0;
    return strings[status];
}

// Return value: the process status
static int spawn_process(
        const test_entry test,
        const int flag_verbose)
{
#ifndef APARSE_PLATFORM_WIN32
    char path[PATH_MAX] = {0};
    int status = 0;
    if(readlink("/proc/self/exe", path, 
                sizeof(path) - 1) == -1)
    {
        error("unable to get the executable path: %s", 
                strerror(errno));
        return -1;
    }

    const char *argv[4] = {0};
    argv[0] = path;
    argv[1] = test.name;
    argv[2] = flag_verbose ? "--verbose" : 0;
    argv[3] = 0;

    pid_t pid = -1;
    if(posix_spawn(&pid, path, NULL, NULL, 
                (char *const *)(uintptr_t)argv, 0) == -1)
    {
        error("unable to create the process: %s", 
                strerror(errno));
        return -1;
    }

    if(waitpid(pid, &status, 0) == -1)
    {
        error("unable to wait for process to complete: %s", 
                strerror(errno));
        return -1;
    }

    return (status >> 8) & 0xff;
#else
#endif
}

static aparse_status g_last_status = APARSE_STATUS_OK;
static void error_callback(
        const aparse_status status, 
        const void* field1, 
        const void* field2, 
        void *userdata)
{
    if(*(int*)userdata)
        error("%s: %s", __func__, aparse_error_msg(status));
    g_last_status = status;
}

typedef struct copy_data { char src[32], dest[32]; } copy_data;
static void dummy_command(void* data) { (void)data; }

int main(int argc, char** argv)
{
    int flag_verbose = 0;
    const char* test_name = 0;
    aparse_arg main_args[] = {
        aparse_arg_string("test_name", &test_name, 
                0, "the test to perform"),
        aparse_arg_option("-v", "--verbose", 
                &flag_verbose, sizeof(flag_verbose),
                APARSE_ARG_TYPE_BOOL, "Toggle verbosity"),
        aparse_arg_end_marker
    };
    if(aparse_parse(argc, argv, main_args, 0, 0) != APARSE_STATUS_OK)
        return 1;
   
    uint8_t buffer[BUFFER_SIZE] = {0};
    aparse_arg copy_subargs[] = {
        aparse_arg_string("file", 0, 32, "Source"),
        aparse_arg_string("dest", 0, 32, "Destionation"),
        aparse_arg_end_marker
    };
    aparse_arg command[] = {
        aparse_arg_subparser("copy", copy_subargs, dummy_command, 
                buffer, sizeof(buffer), 0, copy_data, src, dest),
        aparse_arg_end_marker
    };
    aparse_arg args_1[] = {
        aparse_arg_parser("command", command),
        aparse_arg_end_marker
    };
    
    aparse_arg bignum_args[] = {
        aparse_arg_number("bignum", 
                buffer, 16, 
                APARSE_ARG_TYPE_UNSIGNED, 0),
        aparse_arg_end_marker 
    };
    aparse_arg i64_args[] = {
        aparse_arg_number("num", 
                buffer, sizeof(buffer[0]), 
                APARSE_ARG_TYPE_SIGNED, 0),
        aparse_arg_end_marker
    };
    aparse_arg u64_args[] = {
        aparse_arg_number("num", 
                buffer, sizeof(buffer[0]), 
                APARSE_ARG_TYPE_UNSIGNED, 0),
        aparse_arg_end_marker
    };

    const test_entry tests[] = 
    {
        {
            .name = "null-args", 
            .argc = 1, 
            .argv = (const char*[]){"tests"},
            .args = NULL, 
            .expected = APARSE_STATUS_OK,
            .hash = BUFFER_ZEROED_HASH
        },
        {
            .name = "no-arg", 
            .argc = 1, 
            .argv = (const char*[]){"tests"}, 
            .args = args_1, 
            .expected = APARSE_STATUS_MISSING_POSITIONAL,
            .hash = BUFFER_ZEROED_HASH
        },
        {
            .name = "invalid-cmd", 
            .argc = 2, 
            .argv = (const char*[]){"tests", "a"},
            .args = args_1, 
            .expected = APARSE_STATUS_INVALID_SUBCOMMAND,
            .hash = BUFFER_ZEROED_HASH
        },
        {
            .name = "bignum", 
            .argc = 2, 
            .argv = (const char*[]){"tests", "0"},
            .args = bignum_args, 
            .expected = APARSE_STATUS_UNHANDLED,
            .hash = BUFFER_ZEROED_HASH
        },
        {
            .name = "i64-uf", 
            .argc = 2, 
            .argv = (const char*[]){"tests", "-9223372036854775809"}, 
            .args = i64_args, 
            .expected = APARSE_STATUS_UNDERFLOW,
            .hash = BUFFER_ZEROED_HASH
        },
        {
            .name = "i64-of", 
            .argc = 2, 
            .argv = (const char*[]){"tests", "9223372036854775808"}, 
            .args = i64_args, 
            .expected = APARSE_STATUS_OVERFLOW,
            .hash = BUFFER_ZEROED_HASH
        },
        {
            .name = "u64-uf", 
            .argc = 2, 
            .argv = (const char*[]){"tests", "-1"}, 
            .args = u64_args, 
            .expected = APARSE_STATUS_UNDERFLOW,
            .hash = BUFFER_ZEROED_HASH
        },
        {
            .name = "u64-of", 
            .argc = 2, 
            .argv = (const char*[]){"tests", "18446744073709551616"}, 
            .args = u64_args, 
            .expected = APARSE_STATUS_OVERFLOW,
            .hash = BUFFER_ZEROED_HASH
        },
        {
            .name="valid-cmd", 
            .argc=4, 
            .argv = (const char*[]){"tests", "copy", "fox", "binary"},
            .args = args_1, 
            .expected = APARSE_STATUS_OK,
            .hash = 0xA0A33A83
        },    
    };

    if(!strcmp(test_name, "all"))
    {
        int success_count = 0, failed_count = 0;
        for(size_t i = 0; i < ARRSZ(tests); i++)
        {
            const test_entry *entry = &tests[i];
            int res = 0;
            aparse_status status = APARSE_STATUS_OK;

            res = spawn_process(tests[i], flag_verbose);
            status = (aparse_status)res;
            aparse_status expected_status = tests[i].expected;
            int fail = status != expected_status;
    
            if(fail) 
            {
                error("test %zu (\"%s\"): failed", i + 1, entry->name);
                info("expected: %s, got: %s", 
                        status_string(expected_status), 
                        status_string(status));
                failed_count++;
            } else {
                info("test %zu (\"%s\"): passed", i + 1, entry->name);
                success_count++;
            }
        }

        printf("\n");
        info("summary: %d success, %d failed", 
                success_count, failed_count);
    } else {
        size_t test_idx = SIZE_MAX;
        const test_entry *entry = NULL;
        uint32_t hash = 0;

        for(size_t i = 0; i < ARRSZ(tests); i++)
        {
            if(!strcmp(test_name, tests[i].name))
            {
                test_idx = i;
                break;
            }
        }
        if(test_idx == SIZE_MAX)
        {
            error("unknown test was specified: \"%s\"", test_name);
            info("the available tests were:");
            info(" - all");
            for(size_t i = 0; i < ARRSZ(tests); i++)
                info(" - %s", tests[i].name);
            return 1;
        }

        entry = &tests[test_idx];
        aparse_set_error_callback(error_callback, &flag_verbose);
        aparse_parse(
                entry->argc, (char *const *)(uintptr_t)entry->argv, 
                entry->args, NULL, NULL);
        hash = fnv1a(buffer, sizeof(buffer));
        if(entry->hash != hash)
        {
            error("hash mismatched, expected: 0x%08X, got: 0x%08X",
                    entry->hash, hash);
            return (int)APARSE_STATUS_FAILURE;
        }

        return (int)g_last_status;
    }
    
    return 0;
}
