#include "aparse.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    // Variable
    bool verbose = false;
    int64_t number = 0;
    float constant = -1;
    aparse_list strings = {0};
    const char *lorem = 
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor "
        "incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis "
        "nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. "
        "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore "
        "eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt "
        "in culpa qui officia deserunt mollit anim id est laborum.";

    aparse_arg copy_subargs[] = {
        aparse_arg_string("file", 0, 0, "Source"),
        aparse_arg_string("dest", 0, 0, "Destionation"),
        aparse_arg_end_marker
    };
    aparse_arg main_args[] = {
        aparse_arg_number("number", &number, sizeof(number), APARSE_ARG_TYPE_SIGNED, "Just a number"),
        // array_size=0: take all argument after it. library automatically allocated memory for it
        // element_size=0: means the string have no size limitation
        aparse_arg_array("strings", &strings, sizeof(strings), 0, APARSE_ARG_TYPE_STRING, 0, "An array of strings"),
        aparse_arg_option("-v", "--verbose", &verbose, sizeof(verbose), APARSE_ARG_TYPE_BOOL, "Toggle verbosity"),
        aparse_arg_option("-c", "--constant", &constant, sizeof(constant), 
                APARSE_ARG_TYPE_FLOAT, (char*)lorem),
        aparse_arg_end_marker
    };

    aparse_list dispatch_list = {0};
    if(aparse_parse(argc, argv, main_args, &dispatch_list, 0) != APARSE_STATUS_OK)
        return 1;

    aparse_dispatch_all(&dispatch_list);

    aparse_prog_info("sizeof(copy_subargs): %lu, sizeof(main_args): %lu, total: %lu",
        sizeof(copy_subargs), sizeof(main_args),
        sizeof(copy_subargs) + sizeof(main_args)
    );
    aparse_prog_info("This is an info text");
    aparse_prog_warn("This is a warning text");
    aparse_prog_error("This is an error text");

    // Main logic here...
    aparse_prog_info("Number: %ld", number);
    aparse_prog_info("Constant: %f", constant);
    aparse_prog_info("Verbosity: %d", verbose);
    for(int i = 0; i < strings.size; i++)
        aparse_prog_info("strings[%d]: '%s'", i, aparse_list_get(&strings, i, char*));
    free(strings.ptr); // Deallocate pointer that aparse allocated for us
    return 0;
}
