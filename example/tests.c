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

typedef struct test_entry {
    char* name;
    int argc;
    char* const * argv;
    aparse_arg* args;
    aparse_status expected_status;
    uint32_t* hashs;
} test_entry;


static inline uint32_t fnv1a(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 16777619u;
    }
    return h;
}

const char* status_string(const aparse_status status)
{
    const char* strings[] = {
        "APARSE_STATUS_OK",
        "APARSE_STATUS_FAILURE",
        "APARSE_STATUS_UNKNOWN_ARGUMENT",
        "APARSE_STATUS_INVALID_VALUE",
        "APARSE_STATUS_OVERFLOW",
        "APARSE_STATUS_UNDERFLOW",
        "APARSE_STATUS_MISSING_POSITIONAL",
        "APARSE_STATUS_INVALID_SUBCOMMAND",
        "APARSE_STATUS_NULL_POINTER",
        "APARSE_STATUS_INVALID_TYPE",
        "APARSE_STATUS_INVALID_SIZE",
        "APARSE_STATUS_ALLOC_FAILURE",
        "APARSE_STATUS_UNHANDLED",
    };
    if(status < 0 || status >= __APARSE_STATUS_ENUM_END__)
        return 0;
    return strings[status];
}

// Return value: the process status
static int spawn_process(const test_entry test)
{
#ifndef APARSE_PLATFORM_WIN32
    char path[PATH_MAX] = {0};
    if(readlink("/proc/self/exe", path, sizeof(path) - 1) == -1)
    {
        aparse_prog_error("unable to get the executable path: %s", strerror(errno));
        return -1;
    }

    char *argv[3] = {0};
    argv[0] = path;
    argv[1] = test.name;
    argv[2] = 0;

    pid_t pid = -1;
    if(posix_spawn(&pid, path, 0, 0, argv, 0) == -1)
    {
        aparse_prog_error("unable to create the process: %s", strerror(errno));
        return -1;
    }

    int status = 0;
    if(waitpid(pid, &status, 0) == -1)
    {
        aparse_prog_error("unable to wait for process to complete: %s", strerror(errno));
        return -1;
    }

    return (status >> 8) & 0xff;
#else
#endif
}

static aparse_status last_status = APARSE_STATUS_OK;
static void error_callback(const aparse_status status, const void* field1, const void* field2, void *userdata)
{
    aparse_prog_error("%s: %s", __func__, aparse_error_msg(status));
    if(status == APARSE_STATUS_NULL_POINTER)
    {
        aparse_prog_info("\"%s\" had a null pointer", ((aparse_arg*)field1)->longopt);
    }
    last_status = status;
}

typedef struct copy_data { char* src, *dest; } copy_data;
static int subcommand_status = -1;
void copy_command(void* data)
{
    copy_data* ptr = data;
    uint32_t src_hash = fnv1a(ptr->src);
    uint32_t dest_hash = fnv1a(ptr->dest);
    if(src_hash == 0xB6F3934E && dest_hash == 0xDD856CFC)
        subcommand_status = 1;
    else
        subcommand_status = 0;
}

int main(int argc, char** argv)
{
    int force = 0;
    const char* test_name = 0;
    aparse_arg main_args[] = {
        aparse_arg_string("test_name", &test_name, 0, "the test to perform"),
        aparse_arg_option("-f", "--force", &force, sizeof(force), APARSE_ARG_TYPE_BOOL, "Force running the test whether system endian"),
        aparse_arg_end_marker
    };
    if(aparse_parse(argc, argv, main_args, 0) != APARSE_STATUS_OK)
        return 1;
   
    uint8_t storage[8][8] = {0};
    aparse_arg copy_subargs[] = {
        aparse_arg_string("file", 0, 0, "Source"),
        aparse_arg_string("dest", 0, 0, "Destionation"),
        aparse_arg_end_marker
    };
    aparse_arg command[] = {
        aparse_arg_subparser("copy", copy_subargs, copy_command, 0, copy_data, src, dest),
        aparse_arg_end_marker
    };
    aparse_arg args_1[] = {
        aparse_arg_parser("command", command),
        aparse_arg_end_marker
    };
    
    aparse_arg bignum_args[] = {
        aparse_arg_number("bignum", storage[0], 16, APARSE_ARG_TYPE_UNSIGNED, 0),
        aparse_arg_end_marker 
    };
    aparse_arg i64_args[] = {
        aparse_arg_number("num", storage[0], sizeof(storage[0]), APARSE_ARG_TYPE_SIGNED, 0),
        aparse_arg_end_marker
    };
    aparse_arg u64_args[] = {
        aparse_arg_number("num", storage[0], sizeof(storage[0]), APARSE_ARG_TYPE_UNSIGNED, 0),
        aparse_arg_end_marker
    };

    const test_entry tests[] = {
        {.name="null-args", .argc=1, .argv=(char*[]){"tests"},
            0, APARSE_STATUS_OK},
        {"no-arg", 1, (char*[]){"tests"}, 
            args_1, APARSE_STATUS_MISSING_POSITIONAL},
        {"invalid-cmd", 2, (char*[]){"tests", "a"},
            args_1, APARSE_STATUS_INVALID_SUBCOMMAND},
        {.name="valid-cmd", .argc=4, .argv=(char*[]){"tests", "copy", "fox", "binary"},
            args_1, APARSE_STATUS_OK},    
        {.name="bignum", .argc=2, .argv=(char*[]){"tests", "0"},
                bignum_args, APARSE_STATUS_UNHANDLED},
        {"i64-uf", 2, (char*[]){"tests", "-9223372036854775809"}, 
            i64_args, APARSE_STATUS_UNDERFLOW},
        {"i64-of", 2, (char*[]){"tests", "9223372036854775808"}, 
            i64_args, APARSE_STATUS_OVERFLOW},
        {"u64-uf", 2, (char*[]){"tests", "-1"}, 
            u64_args, APARSE_STATUS_UNDERFLOW},
        {"u64-of", 2, (char*[]){"tests", "18446744073709551616"}, 
            u64_args, APARSE_STATUS_OVERFLOW}
    };
    const int tests_count = sizeof(tests) / sizeof(tests[0]);

    if(!(*(unsigned char *)&(uint16_t){1}) && !force)
    {
        aparse_prog_error("this is only usable on little-endian machine");
        aparse_prog_info("the order of bytes is difference, affect the hash");
        aparse_prog_info("specify \"--force\" to force the test to run, some may fail");
        return 0;
    }
 
    if(!strcmp(test_name, "all"))
    {
        int success_count = 0, failed_count = 0;
        for(int i = 0; i < tests_count; i++)
        {
            int status = spawn_process(tests[i]);
            int expected_status = tests[i].expected_status;
            int fail = status != expected_status;
    
            aparse_prog_info("test %d (\"%s\"): %s", i + 1, tests[i].name, fail ? "failed" : "passed");
            if(fail) {
                failed_count++;
                aparse_prog_info("expected: %s, got: %s", status_string(expected_status), status_string(status));
            } else {
                success_count++;
            }
        }

        printf("\n");
        aparse_prog_info("summary: %d success, %d failed", success_count, failed_count);
    } else {
        int test_idx = -1;
        for(int i = 0; i < tests_count; i++)
        {
            if(!strcmp(test_name, tests[i].name))
            {
                test_idx = i;
                break;
            }
        }
        if(test_idx == -1)
        {
            aparse_prog_error("unable to find the test with the following name: \"%s\"", test_name);
            aparse_prog_info("the available tests were:");
            for(int i = 0; i < tests_count; i++)
                printf(" - %s\n", tests[i].name);
            return 1;
        }

        test_entry entry = tests[test_idx];
        aparse_set_error_callback(error_callback, 0);
        aparse_parse(entry.argc, entry.argv, entry.args, 0);
        return subcommand_status == 0 && subcommand_status != -1 ? 255 : last_status;
    }
    
    return 0;
}
