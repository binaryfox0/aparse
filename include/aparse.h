#ifndef APARSE_H
#define APARSE_H

#include <stdbool.h>

#include "aparse_black_magic.h"

typedef struct aparse_arg_s aparse_arg;
struct aparse_arg_s
{
    char* shortopt;
    char* longopt;
    char* help;
    bool is_positional; // otherwise optional
    bool is_argument; // otherwise parser
    // For positional
    int processed;
    // For argument //
    void* ptr;
    bool is_number;
    int size;
    // For integer
    bool have_sign;
    // For string
    bool auto_allocation;
    // For parser
    aparse_arg* subargs;
    void (*handler)(void* data);
    int* data_layout;
    int layout_size;
};

static inline aparse_arg aparse_arg_option(char* shortopt, char* longopt) {
    return (aparse_arg){.shortopt = shortopt, .longopt = longopt, .is_argument = true};
}

static inline aparse_arg aparse_arg_number(char* name, void* dest, int size, bool have_sign) {
    return (aparse_arg){
        .longopt = name, .is_positional = true, .is_argument = true,
        .ptr = dest, .is_number = true, .size = size, 
        .have_sign = have_sign
    };    
}

static inline aparse_arg aparse_arg_string(char* name, void* dest, int size, bool auto_allocation) {
    return (aparse_arg) {
        .longopt = name, .is_positional = true, .is_argument = true,
        .ptr = dest, .size=size, .auto_allocation = auto_allocation
    };
}

#define __offsetofs(s, ...) { __expand(__map(offsetof, s, __VA_ARGS__)) }
#define aparse_arg_subparser(name, subargs, handle, help, data_struct, ...) \
    aparse_arg_subparser_impl(name, subargs, handle, help, (int[])__offsetofs(data_struct, __VA_ARGS__), __count_args(__VA_ARGS__))
static inline aparse_arg aparse_arg_subparser_impl(
    char* name,aparse_arg* subargs, void (*handle)(void*), 
    char* help, int* data_layout, int layout_size
) {
    return (aparse_arg) {
        .longopt = name, .subargs = subargs, .handler = handle,
        .data_layout = data_layout, .layout_size = layout_size,
        .help = help
    };
}

static inline aparse_arg aparse_arg_parser(char* name, aparse_arg* subparsers) {
    return (aparse_arg){.longopt = name, .subargs = subparsers, .is_positional = true};
}

#define aparse_arg_end_marker (aparse_arg){0}

void aparse_parse(const int argc, char** argv, aparse_arg* args);

#endif