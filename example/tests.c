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

void populate_dest(aparse_arg* main_args, uint8_t** buf,
        const size_t row)
{
    int idx = 0;
    for(aparse_arg* a = main_args; aparse_arg_nend(a); a++)
    {
        
        a->ptr = buf[idx++];
        if(idx >= row)
        {
            aparse_prog_error("insufficient buffer when populating argument's dest");
            return;
        }
    }
}

static aparse_status last_status = APARSE_STATUS_OK;
static void error_callback(const aparse_status status, const void* field1, const void* field2, void *userdata)
{
    aparse_prog_info("%s: %s", __func__, aparse_error_msg(status));
    last_status = status;
}

typedef struct copy_data { char* src, *dest; } copy_data;
void copy_command(void* data)
{
}

int main(int argc, char** argv)
{
    const char* test_name = 0;
    aparse_arg main_args[] = {
        aparse_arg_string("test_name", &test_name, 0, "the test to perform"),
        aparse_arg_end_marker
    };
    if(aparse_parse(argc, argv, main_args, 0) != APARSE_STATUS_OK)
        return 1;
   
    aparse_arg copy_subargs[] = {
        aparse_arg_string("file", 0, 0, "Source"),
        aparse_arg_string("dest", 0, 0, "Destionation"),
        aparse_arg_end_marker
    };
    aparse_arg command[] = {
        aparse_arg_subparser("copy", copy_subargs, copy_command, "Copy file from source to destination", copy_data, src, dest),
        aparse_arg_end_marker
    };
    aparse_arg args_1[] = {
        aparse_arg_parser("command", command),
        aparse_arg_number("number", 0, 2,
                APARSE_ARG_TYPE_SIGNED, "Just a number"),
        aparse_arg_array("strings", 0, 2, APARSE_ARG_TYPE_STRING, 0, "An array of strings"),
        aparse_arg_option("-v", "--verbose", 0, 4,
                APARSE_ARG_TYPE_BOOL, "Toggle verbosity"),
        aparse_arg_option("-c", "--constant", 0, 4,
                APARSE_ARG_TYPE_FLOAT, "Just a constant"),
        aparse_arg_end_marker
    };

    aparse_arg no_args[] = {
        aparse_arg_number("number", 0, 4,
                APARSE_ARG_TYPE_SIGNED, "A signed integer"),
        aparse_arg_end_marker
    };


    const test_entry tests[] = {
        {"no-arg", 1, (char*[]){"tests"}, 
            no_args, APARSE_STATUS_MISSING_POSITIONAL},
        {"invalid-cmd", 2, (char*[]){"tests", "a"},
            args_1, APARSE_STATUS_INVALID_SUBCOMMAND}
    };
    const int tests_count = sizeof(tests) / sizeof(tests[0]);

    if(!strcmp(test_name, "all"))
    {
        for(int i = 0; i < tests_count; i++)
        {
            int status = spawn_process(tests[i]);
            int expected_status = tests[i].expected_status;
            int fail = status != expected_status;
            aparse_prog_info("test %d (\"%s\"): %s", i + 1, tests[i].name, fail ? "failed" : "passed");
            if(fail)
                aparse_prog_info("test %d expected: %d, got: %d", i + 1, status, expected_status);
        }
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
        uint8_t buf[8][8] = {0};
        populate_dest(entry.args, (uint8_t**)buf, sizeof(buf) / sizeof(buf[0]));
        aparse_set_error_callback(error_callback, 0);
        aparse_parse(entry.argc, entry.argv, entry.args, 0);
        return last_status;
    }
    
    return 0;
}
