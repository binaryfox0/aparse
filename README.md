# aparse

A flexible argument parser library for C/C++, inspired by Python's `aparse`. For now, I can say that it supports all of the important features of an argument parser need to have. It supports:

- Positional and optional arguments
- Short (`-f`) and long (`--file`) options
- `key=value` and split-value style (`--file=value` and `--file value`)
- Subcommands (subparsers)
- Automatic help generation
- Type parsing for string/int/unsigned/float
- Array of arguments parsing

## Example
```c
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
    int number = 0;
    float constant = -1;
    char** strings = 0;

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
        aparse_arg_array("strings", &strings, 0, APARSE_ARG_TYPE_STRING, 0, "An array of strings"),
        aparse_arg_option("-v", "--verbose", &verbose, sizeof(verbose), APARSE_ARG_TYPE_BOOL, "Toggle verbosity"),
        aparse_arg_option("-c", "--constant", &constant, sizeof(constant), APARSE_ARG_TYPE_FLOAT, "Just a constant"),
        aparse_arg_end_marker
    };
    printf("program: info: sizeof(copy_subargs): %lu, sizeof(command): %lu, sizeof(main_args): %lu, total: %lu\n",
        sizeof(copy_subargs), sizeof(command), sizeof(main_args),
        sizeof(copy_subargs) + sizeof(command) + sizeof(main_args)
    );
    if(aparse_parse(argc, argv, main_args, "Just an example for repo") != 0)
        return 1;
    // Main logic here...
    printf("Number: %d\n", number);
    printf("Constant: %f\n", constant);
    printf("Verbosity: %d\n", verbose);
    for(int i = 0; i < main_args[2].size && strings; i++)
        printf("strings[%d]: '%s'\n", i, strings[i]);
    if(strings)
        free(strings); // Deallocate pointer that aparse allocated for us
    return 0;
}
```

## Documentation
[View](./DOCS.md) the document here.