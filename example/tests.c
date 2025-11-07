#include <stdio.h>

#include "aparse.h"

typedef struct test_entry {
    const char* name;
    int argc;
    char* const * argv;
    aparse_arg* args;
} test_entry;

static const test_entry entries[] = {
};

static void error_callback(const aparse_status status, const void* field1, const void* field2, void *userdata)
{
    switch(status)
    {
    case APARSE_STATUS_NULL_POINTER:
        aparse_prog_warn("null pointer was expected, continue.");
        break;
    }
}

void run_test(const int argc, const char** argv, const aparse_arg* main_args)
{

}

int main(int argc, char** argv)
{
    const char* test_name = 0;
    aparse_arg main_args[] = {
        aparse_arg_array("test_name", &test_name, sizeof(test_name), APARSE_ARG_TYPE_STRING, 0, 0),
        aparse_arg_end_marker
    };
    if(aparse_parse(argc, argv, main_args, 0) != APARSE_STATUS_OK)
        return 1;
    
    printf("test_name: %s\n", test_name);
    return 0;
}