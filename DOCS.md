> [!IMPORTANT]  
> If `aparse_parse` fails while parsing command-line arguments, it will terminate the program immediately.
Place the argument parsing code before any allocations or stateful operations to prevent memory leaks.

## aparse_arg struct
```c
typedef struct aparse_arg_s aparse_arg;
struct aparse_arg_s
{
    char* shortopt;
    char* longopt;
    char* help;
    aparse_arg_types type;
    uint8_t flags;
    // ==APARSE_ARG_TYPE_ARRAY:   size of requirement member in output array
    // &APARSE_ARG_TYPE_ARGUMENT: size of output variable
    // ~APARSE_ARG_TYPE_ARGUMENT: number of member in struct/pair in data_layout
    int size;
    union {
        // For arguments
        void* ptr;
        // For subparsers/subcommands
        struct {
            aparse_arg* subargs;
            void (*handler)(void* data);
            int* data_layout;
        };
    };
};

```
### Members
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

`aparse_arg_types type`
- The type of arguments (e.g string, integer, float, etc)
- See [enum aparse_arg_types](#enum-aparse_arg_types) for all values

`uint8_t flags`
- Internal parser flags
- Typically **not set manually** by the user

`int size`
- The meaning of size depends on the **argument type**   

**1. Normal arguments (`APARSE_ARG_TYPE_ARGUMENT`)**
- **Definition**: The size of the destination variable (`ptr`) in bytes
- **Special case (strings)**:
  - If `type == APARSE_ARG_TYPE_STRING` **and** ` size == 0`, the parser will automatically assign the `argv[index]` to `*ptr`, don't free it. We expect you pass the ptr as `char**`
- **Recommendation**: Using `sizeof` operator on the output variable.   

**2. Array arguments (`APARSE_ARG_TYPE_ARRAY`)**
- **Definition**: The number of elements in the destination array.     

**3. Subparser arguments (`APARSE_ARG_TYPE_SUBPARSER`)**
- **Definition**: The number of `offset-size` pairs in `data_layout`
### Argument-only fields (`is_argument == true`)
`void* ptr`
- A pointer to the variable where the parsed value will be stored

### Sub-parser only fields
`aparse_arg* subargs`
- Array of subarguments used in the subcommand

`void (*handler)(void* data)`
- Handler will be called after subparser's parsing completed successfully.

`int* data_layout`
- An 1D-array consited of `offset-size` pair
- Specify memory layout for argument data passed to `handler`   
**Example**:
For a struct
```c
struct config {
    int port;
    char *name;
};
```
a layout array might look like:
```c
int layout[] = {
    offsetof(config, port), sizeof(port),
    offsetof(config, name), sizeof(char*)
};
```


## enum aparse_arg_types
```c
typedef enum aparse_arg_types_e aparse_arg_types;
enum aparse_arg_types_e
{
    APARSE_ARG_TYPE_SIGNED_FLAGS = (1 << 7),

    APARSE_ARG_UNKNOWN = 0,
    APARSE_ARG_TYPE_STRING,
    APARSE_ARG_TYPE_BOOL,
    APARSE_ARG_TYPE_UNSIGNED,
    APARSE_ARG_TYPE_FLOAT,
    APARSE_ARG_TYPE_ARRAY = (1 << 3),

    APARSE_ARG_TYPE_POSITIONAL = (1 << 4),
    APARSE_ARG_TYPE_ARGUMENT   = (1 << 5),
    APARSE_ARG_TYPE_SUBPARSER  = APARSE_ARG_TYPE_POSITIONAL,
    
    APARSE_ARG_TYPE_SIGNED = APARSE_ARG_TYPE_UNSIGNED | APARSE_ARG_TYPE_SIGNED_FLAGS,

    APARSE_ARG_TYPE_BITMASK = 0b111
};
```

## Functions
### aparse_arg_option
```c
APARSE_INLINE aparse_arg aparse_arg_option(char* shortopt, char* longopt, void* dest, int size, bool is_number, bool have_sign, bool negatable, char* help)
```
Create a aparse_arg entry for optional arguments
### Paramaters
- [in, opt] **shortopt** The short option name     
- [in, opt] **longopt**  The long option  name     
- [in] **dest** Pointer to variable where value will be stored     
- [in] **size** Size of variable in bytes. [See more](#argument-only-fields-is_argument--true) at size member     
- [in] **is_number** Specify the type of the taken argument     
- [in] **have_sign** Specify signed/unsigned of integer     
- [in] **negetable** Specify if `dest` will change value from `true` to `false` and otherwise    
- [in, opt] **help** Help message for this argument     
### Note
- one of `shortopt` or `longopt` must not NULL, otherwise it will be considered as the end marker     

`APARSE_INLINE aparse_arg aparse_arg_number(char* name, void* dest, int size, bool have_sign, char* help)`     
Create a aparse_arg entry for positional numerical argument
### Paramaters
- [in] **name** Name for this argument
- [in] **dest** Pointer to variable where value will be stored
- [in] **size** Size of the variable in bytes
- [in] **have_sign** Specify signed/unsigned of integer
- [in, opt] **help** Help message for this message

`APARSE_INLINE aparse_arg aparse_arg_string(char* name, void* dest, int size, char* help)`      
Create a aparse_arg entry for positional string value
### Paramaters
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
### Paramaters
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
### Paramaters
- [in] **name** Name for this parser
- [in] **subargs** An array of subcommands

`aparse_arg_end_marker`
Create a aparse_arg that marks the end of an array of aparse_arg

`int aparse_parse(const int argc, char** argv, aparse_arg* args, const char* program_desc)`
Parse the command line
### Paramaters
- [in] **argc** Number of arguments in `argv`
- [in] **argv** List of argument string
- [in] **args** An array of aparse_arg use for parsing command line
- [in, opt] **program_desc** Description for the purpose of program

### Return value
- [out] status for parsing procedure, 0 if success and APARSE_ABORT for failure