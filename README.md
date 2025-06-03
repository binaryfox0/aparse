# aparse

A flexible argument parser library for C/C++, inspired by Python's `aparse`. For now, I can say that it supports all of the important features of an argument parser need to have. It supports:

- Positional and optional arguments
- Short (`-f`) and long (`--file`) options
- `key=value` and split-value style (`--file=value` and `--file value`)
- Subcommands (subparsers)
- Automatic help generation
- Type parsing for string/int/unsigned

## Example
```c
#include "aparse.h"

typedef struct { char* src, *dest; } copy_data;
void install_command(void* data) {
    copy_data* processed = data;
    // Processing here...
}

int main(int argc, char** argv) {
    // Variable
    bool verbose = false;
    aparse_arg copy_subargs[] = {
        aparse_arg_string("file", 0, 0, 1, "Source"),
        aparse_arg_string("dest", 0, 0, 1, "Destionation"),
        aparse_arg_end_marker
    };
    aparse_arg command[] = {
        aparse_arg_subparser("copy", copy_subargs, install_command, "", copy_data, file, dest),
        aparse_arg_end_marker
    };
    aparse_arg main_args[] = {
        aparse_add_parser("command", command),
        aparse_add_option("-v", "--verbose", &verbose, sizeof(verbose), true, false),
        aparse_arg_end_marker
    };
    aparse_parse(argc, argv, main_args, "Just an example for repo");
    // Main logic here
    return 0;
}
```

## Documentation
> [!IMPORTANT]  
> If aparse_parse parse command line failed, it will terminate program immediately. You should place the argument parsing code before anything else to pervent any memory leak.
### Will add later.