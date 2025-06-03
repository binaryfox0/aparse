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
        aparse_arg_subparser("copy", copy_subargs, install_command, "Copy file from source to destination", copy_data, file, dest),
        aparse_arg_end_marker
    };
    aparse_arg main_args[] = {
        aparse_add_parser("command", command),
        aparse_add_option("-v", "--verbose", &verbose, sizeof(verbose), true, false, "Toggle verbosity"),
        aparse_arg_end_marker
    };
    aparse_parse(argc, argv, main_args, "Just an example for repo");
    // Main logic here...
    return 0;
}
```

## Documentation
> [!IMPORTANT]  
> If aparse_parse parse command line failed, it will terminate program immediately. You should place the argument parsing code before anything else to pervent any memory leak.
### aparse_arg struct
```c
typedef struct aparse_arg_s aparse_arg;
struct aparse_arg_s
{
    char* shortopt;
    char* longopt;
    char* help;
    bool is_positional; // otherwise optional
    bool is_argument; // otherwise parser
    uint8_t flags;
    // For argument //
    union {
        struct {
            void* ptr;
            bool is_number;
            int size;
            // For integer
            bool have_sign;
            // For string
            bool auto_allocation;
            bool allocated;
        };
        // For subparsers/subcommands
        struct {
            aparse_arg* subargs;
            void (*handler)(void* data);
            int* data_layout;
            int layout_size;
        };
    };
    // For optional
    bool negatable;
};
```
#### Members
`char* shortopt`
- The short option (e.g, `-h`) name for optional arguments.
- **Ignored** for optional arguments
- May be `NULL` if `longopt` is used instead  

`char* longopt`
- The long option name (e.g, `--help`) for optional arguments
- **Required** for positional arguments
- May be `NULL` if `shortopt` is present and sufficient.

`char* help`
- A help message describing the argument.
- Displayed in help outputs.

`bool is_positional`
- If `true`, the argument is **positional** and its value is parsed based on its order in the command line
- If `false`, the argument is **optional** and identified by `shortopt` or `longopt`

`bool is_argument`
- If `true`, this entry is a **standard argument** (i.e., value stored at `ptr`)
- If `false`, this entry is a **parser** or **subcommand**, which contains nested arguments (`subargs`) and a handler

`uint8_t flags`
- Arbitary flags used internally by the parser itself to track parsing
#### Argument-only fields (`is_argument == true`)
`void* ptr`
- A pointer to the variable where the parsed value will be stored

`bool is_number`
- If `true`, argument is a numeric value, `size` and `have_sign` are relevant
- If `false`, argument is a string, `auto_allocation` is relevant

`int size`
- **Recommendation**: Use sizeof operator for numerical value
- Size in bytes of the variable where the `ptr` point to. For string it will include the null-terminator (`\0`)
- For `is_number == false`, size = 0 means that it will have dynamic size. It really means that *ptr is `char*` and will be assigned with specific argv

`bool have_sign`
- If `true`, the numeric type is **signed**
- If `false`, it is unsigned

#### Sub-parser only fields
`aparse_arg* subargs`
- Array of subarguments used in the subcommand

`void (*handler)(void* data)`
- Handler to process data after successful parsing.
- **Need to add note**

`int* data_layout`
- An 1D-array consited of `offset-size` pair
- Specify memory layout for argument data passed to `handler`

`int layout_size`
- Number of pairs in `data_layout`

#### Optional-only field (`is_positional == false`)
`bool negatble`
- If `true`, boolean flags can be negated. For example, if value of ptr is true, it will be false
- Applies only to boolean/integer arguments

### Functions
`APARSE_INLINE aparse_arg aparse_arg_option(char* shortopt, char* longopt, void* dest, int size, bool is_number, bool have_sign, bool negatable, char* help)`         
Create a aparse_arg entry for optional arguments
#### Paramaters
- [in, opt] **shortopt** The short option name     
- [in, opt] **longopt**  The long option  name     
- [in] **dest** Pointer to variable where value will be stored     
- [in] **size** Size of variable in bytes. [See more](#argument-only-fields-is_argument--true) at size member     
- [in] **is_number** Specify the type of the taken argument     
- [in] **have_sign** Specify signed/unsigned of integer     
- [in] **negetable** Specify if `dest` will change value from `true` to `false` and otherwise    
- [in, opt] **help** Help message for this argument     
#### Note
- one of `shortopt` or `longopt` must not NULL, otherwise it will be considered as the end marker     

`APARSE_INLINE aparse_arg aparse_arg_number(char* name, void* dest, int size, bool have_sign, char* help)`     
Create a aparse_arg entry for positional numerical argument
#### Paramaters
- [in] **name** Name for this argument
- [in] **dest** Pointer to variable where value will be stored
- [in] **size** Size of the variable in bytes
- [in] **have_sign** Specify signed/unsigned of integer
- [in, opt] **help** Help message for this message

`APARSE_INLINE aparse_arg aparse_arg_string(char* name, void* dest, int size, char* help)`      
Create a aparse_arg entry for positional string value
#### Paramaters
- [in] **name** Name for this argument
- [in] **dest** Pointer to variable where value will be stored
- [in] **size** Size of the variable in bytes
- [in, opt] **help** Help message for this message

`aparse_arg_subparser(name, subargs, handle, help, data_struct, ...)`      
Create a aparse_arg entry for a subparser. A wrapper for aparse_arg_subparse_impl      
`APARSE_INLINE aparse_arg aparse_arg_subparser_impl(
    char* name,aparse_arg* subargs, void (*handle)(void*), 
    char* help, int* data_layout, int layout_size
)`
#### Paramaters
- [in] **name** Name for this subcommands
- [in] **subargs** Array of subarguments used in this subcommand
- [in] **handle** Handle to be called when parsing successfully
- [in, opt] **help** Help message for this subcommands
- [in] **data_struct** Struct type to generate `data_layout`
- [in] **...** Member in struct you want argument value to be mapped
   - [in] **data_layout** An array stored information about data mapping. Store a list of pair `offset, size` flatten to 1D array
   - [in] **layout_size** Number of pairs in `data_layout`

`APARSE_INLINE aparse_arg aparse_arg_parser(char* name, aparse_arg* subparsers)`
Create a aparse_arg entry for parser
#### Paramaters
- [in] **name** Name for this parser
- [in] **subargs** An array of subcommands

`aparse_arg_end_marker`
Create a aparse_arg that marks the end of an array of aparse_arg

`void aparse_parse(const int argc, char** argv, aparse_arg* args, const char* program_desc)`
Parse the command line
#### Paramaters
- [in] **argc** Number of arguments in `argv`
- [in] **argv** List of argument string
- [in] **args** An array of aparse_arg use for parsing command line
- [in, opt] **program_desc** Description for the purpose of program
