#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#include "aparse.h"
#include "../src/aparse_list.h"

static inline bool aparse_arg_nend(const aparse_arg* arg) {
    return arg->longopt != 0 || arg->shortopt != 0;
}


#define INDENT 2 // indent/space
#define MAX_ARG_STR 19
// 22 space from the left side to help
void aparse_construct_help_tag(const aparse_arg* arg, int indent) {
    if(!arg->is_argument && !arg->help && !aparse_arg_nend(arg)) return;
    int len = printf("%*s%s%s%s", 
        indent, "", 
        arg->shortopt ? arg->shortopt : "", 
        arg->shortopt ? ", " : "",
        arg->longopt  ? arg->longopt  : ""
    ) - INDENT;
    bool longer = len > MAX_ARG_STR;
    if(arg->help) {
        int space = INDENT + (longer ? INDENT + MAX_ARG_STR : MAX_ARG_STR - len);
        printf("%s%*s%s", longer ? "\n" : "", space, "", arg->help);
    }
    printf("\n");
}

void aparse_print_available_subcommands(const aparse_arg* args) {
    printf("{");
    int index = 0;
    for (aparse_arg* ptr = args->subargs; aparse_arg_nend(ptr); ptr++) {
        if(index > 0)
            printf(", ");
        printf("%s", ptr->longopt);
        index++;
    }
    printf("}");
}

void aparse_construct_positional_help(aparse_arg* args) {
    for (; aparse_arg_nend(args); args++) {
        if (args->is_positional) {
            if(args->is_argument) {
                aparse_construct_help_tag(args, INDENT);
                continue;
            }
            printf("%*s", INDENT, "");
            aparse_print_available_subcommands(args);
            printf("\n");

            for (aparse_arg* ptr = args->subargs; aparse_arg_nend(ptr); ptr++)
                aparse_construct_help_tag(ptr, INDENT * 2);
        }
    }
}




void aparse_construct_usage(aparse_arg* args) {
    aparse_list list;
    aparse_list_new(&list, 0, sizeof(aparse_arg*));
    for(aparse_arg* real = args; aparse_arg_nend(real); real++) {
        if(real->is_positional) {
            if(!real->is_argument) {
                aparse_print_available_subcommands(real);
                printf(" ... ");
            } else {
                printf("%s ", real->longopt);
            }
        } else {
            aparse_list_add(&list, real);
        }
    }
    for(size_t i = 0; i < list.size; i++) {
        aparse_arg* entry = aparse_list_get(&list, i);
        printf("[%s] ", entry->shortopt ? entry->shortopt : entry->longopt);
    }
    aparse_list_free(&list);
    printf("\n");
}

void aparse_construct_help(aparse_arg* args) {
    printf("positional arguments:\n");
    aparse_construct_positional_help(args);
    printf("\noptions:\n");
    for(aparse_arg* real = args; aparse_arg_nend(real); real++) {
        if(real->is_argument && !real->is_positional)
            aparse_construct_help_tag(real, INDENT);
    }
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
    aparse_arg subinstall[] = {
        aparse_arg_string("file", 0, 0, 1),
        aparse_arg_string("dest", 0, 0, 1),
        aparse_arg_end_marker
    };
    aparse_arg install[] = {
        // aparse_arg_string("file", 0, 0, 1),
        // aparse_arg_string("dest", 0, 0, 1),
        aparse_arg_parser("command2", (aparse_arg[]){
            aparse_arg_subparser("file", subinstall, install_command, "", install_struct, file, dest),
            aparse_arg_end_marker
        }),
        aparse_arg_end_marker
    };
    aparse_arg command[] = {
         aparse_arg_subparser("install", install, install_command, "Install package", install_struct, file, dest),
        //aparse_arg_subparser_impl("install", install, 0, "", 0, 0),
        aparse_arg_end_marker
    };
    int number = 0;
    aparse_arg main_args[] = {
        aparse_arg_parser("command", command),
        aparse_arg_number("number", &number, 4, 1),
        aparse_arg_option("--output", "-o"),
        aparse_arg_end_marker
    };
    aparse_parse(argc, argv, main_args);
    // aparse_construct_help(main_args);
    // aparse_construct_usage(install);
    printf("%d\n", number);
    return 0;
}