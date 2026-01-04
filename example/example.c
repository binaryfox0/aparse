#include "aparse.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct copy_data { char* src, *dest; } copy_data;
void copy_command(void* data) {
    copy_data* processed = data;
    printf("Source: %s, Destionation: %s\n", processed->src, processed->dest);
    // Processed here...
}

int main(int argc, char** argv) {
    // Variable
    bool verbose = false;
    int64_t number = 0;
    float constant = -1;
    aparse_list strings = {0};

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
        aparse_arg_number("number", &number, sizeof(number), APARSE_ARG_TYPE_SIGNED, "Just a number"),
        // array_size=0: take all argument after it. library automatically allocated memory for it
        // element_size=0: means the string have no size limitation
        aparse_arg_array("strings", &strings, sizeof(strings), 0, APARSE_ARG_TYPE_STRING, 0, "An array of strings"),
        aparse_arg_option("-v", "--verbose", &verbose, sizeof(verbose), APARSE_ARG_TYPE_BOOL, "Toggle verbosity"),
        aparse_arg_option("-c", "--constant", &constant, sizeof(constant), APARSE_ARG_TYPE_FLOAT, "Just a constant"),
        aparse_arg_end_marker
    };

    aparse_list dispatch_list = {0};
    if(aparse_parse(argc, argv, main_args, &dispatch_list, 0) != APARSE_STATUS_OK)
        return 1;

    aparse_dispatch_all(&dispatch_list);

    aparse_prog_info("sizeof(copy_subargs): %lu, sizeof(command): %lu, sizeof(main_args): %lu, total: %lu",
        sizeof(copy_subargs), sizeof(command), sizeof(main_args),
        sizeof(copy_subargs) + sizeof(command) + sizeof(main_args)
    );

    // Main logic here...
    aparse_prog_info("Number: %ld", number);
    aparse_prog_info("Constant: %f", constant);
    aparse_prog_info("Verbosity: %d", verbose);
    for(int i = 0; i < strings.size; i++)
        aparse_prog_info("strings[%d]: '%s'", i, *(char**)aparse_list_get(&strings, i));
    free(strings.ptr); // Deallocate pointer that aparse allocated for us
    return 0;
}
