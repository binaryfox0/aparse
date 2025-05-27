#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#include "aparse.h"

static inline bool aparse_arg_nend(aparse_arg* arg) {
    return arg->longopt != 0 || arg->shortopt != 0;
}

#define INDENT 2 // indent/space
void aparse_construct_positional_help_tag(const aparse_arg arg, int indent) {
    if(!arg.is_argument && !arg.help) return;
    printf("%*s%s", indent, "", arg.longopt);
    
    printf("\n");
}

// note: 2 space until help
void aparse_construct_positional_help(aparse_arg* args) {
    for (; aparse_arg_nend(args); args++) {
        if (args->is_positional) {
            if(args->is_argument) {
                aparse_construct_positional_help_tag(*args, INDENT);
                continue;
            }
            printf("%*s{", INDENT, "");
            for (aparse_arg* ptr = args->subargs; aparse_arg_nend(ptr); ptr++)
                printf("%s%s", ptr->longopt, aparse_arg_nend(ptr + 1) ? ", " : "");
            printf("}\n");

            for (aparse_arg* ptr = args->subargs; aparse_arg_nend(ptr); ptr++)
                aparse_construct_positional_help_tag(*ptr, INDENT * 2);
        }
    }
}


void aparse_construct_help(aparse_arg* args) {
    printf("positional arguments:\n");
    aparse_construct_positional_help(args);
}

typedef struct install_struct {
    char* file;
    char* dest;
} install_struct;

void install_command(void* data){
    install_struct processed = *(install_struct*)data;
    printf("file: %s\n", processed.file);
    printf("dest: %s\n", processed.dest);
}

int main(int argc, char** argv)
{
    aparse_arg install[] = {
        aparse_arg_string("file", 0, 0, 1),
        aparse_arg_string("dest", 0, 0, 1),
        aparse_arg_end_marker
    };
    aparse_arg command[] = {
        aparse_arg_subparser("install", install, install_command, install_struct, file, dest),
        aparse_arg_end_marker
    };
    int number = 0;
    aparse_arg main_args[] = {
        aparse_arg_parser("command", command),
        aparse_arg_number("number", &number, 4, 1),
        // aparse_arg_option("--output", "-o"),
        aparse_arg_end_marker
    };
    // aparse_parse(argc, argv, main_args);
     aparse_construct_help(main_args);
    //printf("%d\n", number);
    return 0;
}