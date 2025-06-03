#include "aparse.h"

#include <stdio.h>

typedef struct copy_data { char* src, *dest; } copy_data;
void copy_command(void* data) {
    copy_data* processed = data;
    // Processing here...
}

int main(int argc, char** argv) {
    // Variable
    bool verbose = false;
    aparse_arg copy_subargs[] = {
        aparse_arg_string("file", 0, 0, "Source"),
        aparse_arg_string("dest", 0, 0, "Destionation"),
        aparse_arg_end_marker
    };
    aparse_arg command[] = {
        aparse_arg_subparser("copy", copy_subargs, copy_command, "Copy file from source to destination", copy_data, src, dest),
        aparse_arg_end_marker
    };
    aparse_arg main_args[] = {
        aparse_arg_parser("command", command),
        aparse_arg_option("-v", "--verbose", &verbose, sizeof(verbose), true, false, true, "Toggle verbosity"),
        aparse_arg_end_marker
    };
    aparse_parse(argc, argv, main_args, "Just an example for repo");
    // Main logic here...
    printf("Verbosity: %d\n", verbose);
    return 0;
}