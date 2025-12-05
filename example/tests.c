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

    size_t arrsz = 3 + test.argc;
    char** argv = malloc(sizeof(char*) * arrsz);
    if(!argv)
    {
        aparse_prog_error("unable to allocate memory to spawn process");
        return -1;
    }
    argv[0] = path;
    argv[1] = test.name;
    for(int i = 0; i < test.argc; i++)
        argv[i + 2] = test.argv[i];
    argv[arrsz - 1] = 0;

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

    return status;
#else
#endif
}

static void error_callback(const aparse_status status, const void* field1, const void* field2, void *userdata)
{
    aparse_prog_info("%s: %s", __func__, aparse_error_msg(status));
}

int main(int argc, char** argv)
{
    const char* test_name = 0;
    char** tests_arg = 0;
    aparse_arg main_args[] = {
        aparse_arg_string("test_name", &test_name, 0, "the test to perform"),
        aparse_arg_array("argv", &tests_arg, 0, APARSE_ARG_TYPE_STRING, 0, "arguments for the specified test"),
        aparse_arg_end_marker
    };
    if(aparse_parse(argc, argv, main_args, 0) != APARSE_STATUS_OK)
        return 1;

    const test_entry tests[] = {
        {"no-arg", }
    };
    const int tests_count = sizeof(tests) / sizeof(tests[0]);

    if(!strcmp(test_name, "all"))
    {
        for(int i = 0; i < tests_count; i++)
        {
            int status = spawn_process(tests[i]);
            aparse_prog_info("test %d (\"%s\"): %s, status: %d", i + 1, tests[i].name, status == 0 ? "passed" : "failed", status);
        }
    } else {
        int test_found = 0;
        for(int i = 0; i < tests_count; i++)
        {
            if(!strcmp(test_name, tests[i].name))
            {
                test_found = 1;
                break;
            }
        }
        if(!test_found)
        {
            aparse_prog_error("unable to find the test with the following name: \"%s\"", test_name);
            aparse_prog_info("the available tests were:");
            for(int i = 0; i < tests_count; i++)
                printf(" - %s\n", tests[i].name);
            return 1;
        }
    }
    
    return 0;
}