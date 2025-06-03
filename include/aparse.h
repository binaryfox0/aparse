/*
MIT License

Copyright (c) 2025 binaryfox0

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef APARSE_H
#define APARSE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "aparse_black_magic.h"

#if defined(_MSC_VER)
#   define APARSE_INLINE __forceinline
#else
#   define APARSE_INLINE static inline __attribute((always_inline))
#endif

#ifdef __cplusplus
extern "C" {
#endif

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

APARSE_INLINE aparse_arg aparse_arg_option(char* shortopt, char* longopt, void* dest, int size, bool is_number, bool have_sign, bool negatable, char* help) {
    return (aparse_arg){
        .shortopt = shortopt, .longopt = longopt, .is_argument = true,
        .is_number = is_number, .ptr = dest, .size = size,
        .have_sign = have_sign, .help = help, 
        .negatable = negatable
    };
}

APARSE_INLINE aparse_arg aparse_arg_number(char* name, void* dest, int size, bool have_sign, char* help) {
    return (aparse_arg){
        .longopt = name, .is_positional = true, .is_argument = true,
        .ptr = dest, .is_number = true, .size = size, 
        .have_sign = have_sign, .help = help
    };    
}

APARSE_INLINE aparse_arg aparse_arg_string(char* name, void* dest, int size, char* help) {
    return (aparse_arg) {
        .longopt = name, .is_positional = true, .is_argument = true,
        .ptr = dest, .size=size,
        .help = help,
    };
}

#define offsetof_and_sizeof(s, m) offsetof(s, m), sizeof(((s*)0)->m )
#define __offsetofs(s, ...) { __expand(__map(offsetof_and_sizeof, s, __VA_ARGS__)) }
#define aparse_arg_subparser(name, subargs, handle, help, data_struct, ...) \
    aparse_arg_subparser_impl(name, subargs, handle, help, (int[])__offsetofs(data_struct, __VA_ARGS__), __count_args(__VA_ARGS__))
APARSE_INLINE aparse_arg aparse_arg_subparser_impl(
    char* name,aparse_arg* subargs, void (*handle)(void*), 
    char* help, int* data_layout, int layout_size
) {
    return (aparse_arg) {
        .longopt = name, .subargs = subargs, .handler = handle,
        .data_layout = data_layout, .layout_size = layout_size,
        .help = help,
        .is_positional = 1 // parser doesn't care 'bout this, but help constructor DOES care
    };
}

APARSE_INLINE aparse_arg aparse_arg_parser(char* name, aparse_arg* subparsers) {
    return (aparse_arg){.longopt = name, .subargs = subparsers, .is_positional = true};
}

#define aparse_arg_end_marker (aparse_arg){0}

void aparse_parse(const int argc, char** argv, aparse_arg* args, const char* program_desc);

#ifdef __cplusplus 
}
#endif

#endif