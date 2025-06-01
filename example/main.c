#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#include "aparse.h"

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
    int number = 0;

    aparse_arg subinstall[] = {
        aparse_arg_string("file", 0, 0, 1),
        aparse_arg_string("dest", 0, 0, 1),
        // aparse_arg_option("-o", "--output", &number, 4, true, true),

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
    aparse_arg main_args[] = {
        aparse_arg_number("number2", &number, 4, 1),
        aparse_arg_number("number3", &number, 4, 1),
        aparse_arg_parser("command", command),
        aparse_arg_number("number", &number, 4, 1),
        aparse_arg_option("-o", "--output", &number, 4, true, true),
        aparse_arg_end_marker
    };
    aparse_parse(argc, argv, main_args);
    // printf("%d\n", number);
    return 0;
}