#include "aparse.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "aparse_list.h"

// Helper macro
#define BITMASK_0 (1 << 0)
#define BITMASK_1 (1 << 1)
#define BITMASK_2 (1 << 2)

#define INDENT 2 // indent/space
#define MAX_ARG_STR 19

// Positional
// processed bitmask_0
// Optional
// has_equal bitmask_0
// short_match bitmask_1

#define APARSE_LITTLE_ENDIAN (*(unsigned char *)&(uint16_t){1})
#define APARSE_ALLOC_SIZE 5

#define min(a, b) ((a < b) ? (a) : (b))

typedef struct aparse_internal
{
    int positional_count;
    int positional_processed;
    aparse_list unknown;
    bool is_sublayer;
} aparse_internal;

bool is_valid_number(char* str, bool allow_sign)
{
    if(!str) return 0;
    if(!*str) return 0;

    if(*str == '+' || (allow_sign && *str == '-'))
        str++;
    for(; *str; str++)
        if(!isdigit((uint8_t)*str))
            return false;
    return true;
}

static inline bool aparse_arg_nend(const aparse_arg* arg) {
    return arg->longopt != 0 || arg->shortopt != 0;
}

static inline int aparse_args_positional_count(aparse_arg* args)
{
    int count = 0;
    for(; aparse_arg_nend(args); args++)
        if(args->is_positional)
            count++;
    return count;
}

static char* progname = 0;
static aparse_arg help_arg = { .shortopt = "-h", .longopt = "--help", .help = "show this help message and exit" };

// Forward declaration
int aparse_parse_private(const int argc, char** argv, aparse_arg* args, int* index, aparse_internal* internal);
// The command handler for subparser will be called after the main parsing completed
static aparse_list call_list;
typedef struct call_struct {
    aparse_arg* args;
    void* data;
} call_struct;
// Processing each type of argument
bool aparse_process_argument(char* argv, const aparse_arg* arg);
void aparse_process_parser(const int argc, const char* cargv, char** argv, const aparse_arg* arg, int* index);
bool aparse_process_optional(int argc, char** argv, int* index, const aparse_arg* arg);
// For aparse_arg is subparsers and have proper data_layout & layout_size
char* aparse_compose_data(const aparse_arg* args);
void aparse_free_composed(char* data, aparse_arg* args);
// Failure handling
int aparse_process_failure(const bool status, const aparse_internal internal, aparse_arg* args);
void aparse_required_message(aparse_arg* args);
void aparse_unknown_message(const aparse_internal internal);
// Help-related functions
void aparse_print_help(aparse_arg* main_args);
void aparse_print_help_tag(const aparse_arg* arg, int indent);
void aparse_print_positional_help(aparse_arg* args);
void aparse_print_available_subcommands(const aparse_arg* args);
// Miscellaneous
aparse_arg* aparse_argv_matching(const char* argv, aparse_arg* args);

void aparse_parse(const int argc, char** argv, aparse_arg* args)
{
    progname = argv[0];
    aparse_internal internal = { aparse_args_positional_count(args), 0, {0, 0, 0}};
    aparse_list_new(&internal.unknown, 0, sizeof(char*));
    aparse_list_new(&call_list, 0, sizeof(call_struct));
    int index = 1;
    if(aparse_process_failure(aparse_parse_private(argc, argv, args, &index, &internal) > 0, internal, args)) {
        aparse_list_free(&internal.unknown);
        exit(EXIT_FAILURE);
    }
    for(size_t i = 0; i < call_list.size; i++) {
        call_struct* tmp = (call_struct*)aparse_list_get(&call_list, i);
        tmp->args->handler(tmp->data);
        aparse_free_composed(tmp->data, tmp->args->subargs);
    }
    aparse_list_free(&call_list);
    aparse_list_free(&internal.unknown);
}

// --------------------------------------- PRIVATE ---------------------------------------
int aparse_parse_private(const int argc, char** argv, aparse_arg* args, int* index, aparse_internal* internal)
{
    while (*index < argc) {
        int found = 0;
        char* cargv = argv[*index];
        (*index)++;

        aparse_arg* ptr = aparse_argv_matching(cargv, args);
        if(ptr) {
            if(ptr->is_positional) {
                internal->positional_processed++;
                ptr->flags |= BITMASK_0;
                if(ptr->is_argument) {
                    if(!aparse_process_argument(cargv, ptr))
                        return 1;
                } else 
                    aparse_process_parser(argc, cargv, argv, ptr, index);
            } else {
                if(ptr->shortopt != help_arg.shortopt) {
                    if(!aparse_process_optional(argc, argv, index, ptr))
                        return 1;
                } else {
                    aparse_print_help(args);
                }
            }
        } else {
            if (aparse_list_add(&internal->unknown, cargv)) {
                fprintf(stderr, "aparse: error: failed to allocate memory for parsing process. Retry again.\n");
                return 1;
            }

            if (internal->is_sublayer) {
                (*index)--; 
                return -1;
            }
        }
    }

    return 0;
}


bool aparse_process_argument(char* argv, const aparse_arg *arg) {
    if (!arg->ptr) return true;

    if (arg->is_number) {
        if(!is_valid_number(argv, arg->have_sign)) {
            fprintf(stderr, "%s: error: invalid number '%s'\n", progname, argv);
            return false;
        }
        uint64_t num = arg->have_sign ? (int64_t)strtoll(argv, NULL, 10) : strtoull(argv, NULL, 10);
        memcpy(arg->ptr, &num, min(arg->size, 8));

        // Directly manipulating the sign bit (MSB)
        if (arg->have_sign) {
            uint8_t* b = (uint8_t*)arg->ptr + (APARSE_LITTLE_ENDIAN ? 3 : 0);
            *b = (*b & ~0x80) | (((int64_t)num < 0) ? 0x80 : 0);
        }
    } else {
        size_t len = strlen(argv) + 1;

        if (arg->auto_allocation) {
            char** dst = (char**)arg->ptr;
            char* tmp = realloc(*dst, len);
            if (!tmp) {
                fprintf(stderr, "%s: error: failure to allocate memory for parsing procedure. Retry again.\n", progname);
                return false;
            }
            *dst = tmp;
            memcpy(*dst, argv, len);
        } else {
            size_t n = min(arg->size - 1, len);
            memcpy(arg->ptr, argv, n);
            ((char*)arg->ptr)[n] = '\0';
        }
    }
    return true;
}

void aparse_process_parser(const int argc, const char* cargv, char** argv, const aparse_arg* arg, int* index)
{
    // Find subcommand for parser
    aparse_arg* ptr2 = arg->subargs;
    int found2;
    for(; aparse_arg_nend(ptr2); ptr2++) {
            if(found2 = !strcmp(cargv, ptr2->longopt))
                break;
        }
    if(!found2) {
        fprintf(stderr, "%s: error: invalid choice: '%s' (choose from ", progname, cargv);
        for(aparse_arg* ptr2 = arg->subargs; aparse_arg_nend(ptr2); ptr2++) {
            fprintf(stderr, "%s%s", ptr2->longopt, aparse_arg_nend(ptr2 + 1) ? ", " : "");
        }
        fprintf(stderr, ")\n");
        exit(EXIT_FAILURE);
    }
    // Found it!, compose struct for it to handler process.
    char* data = aparse_compose_data(ptr2);
    aparse_internal internal2 = { aparse_args_positional_count(ptr2->subargs), 0, {0}, 1};
    aparse_list_new(&internal2.unknown, 0, sizeof(char*));
    if(aparse_process_failure(aparse_parse_private(argc, argv, ptr2->subargs, index, &internal2) > 0, internal2, ptr2->subargs)) {
        aparse_free_composed(data, ptr2->subargs);
        aparse_list_free(&internal2.unknown);
        exit(EXIT_FAILURE);
    }
    if(!ptr2->handler)
        aparse_free_composed(data, ptr2->subargs);
    else
        aparse_list_add(&call_list, (call_struct[1]){{ptr2, data}});
    aparse_list_free(&internal2.unknown);
}

bool aparse_process_optional(int argc, char** argv, int* index, const aparse_arg* arg){
    if(arg->flags & BITMASK_0)
        return aparse_process_argument(argv[*index - 1] + 1 + strlen(arg->flags & BITMASK_1 ? arg->shortopt : arg->longopt), arg);
    else {
        if(*index >= argc) {
            printf("%s: error: option '%s' expected 1 argument.\n", progname, arg->flags & BITMASK_1 ? arg->shortopt : arg->longopt);
            return false;
        }
        (*index)++;
        return aparse_process_argument(argv[*index - 1], arg);
    }
}

// Use to create a bytes-like structure to hold data before parsing then pass into handler
char* aparse_compose_data(const aparse_arg* args)
{
    if(!args->subargs || !args->handler)
        return 0;
    aparse_arg* real = args->subargs;
    // Query size
    int index = 0, count = 0;
    for(; aparse_arg_nend(real); real++)
    {
        if(count == args->layout_size)
            break;
        if(!real->is_argument || !real->is_positional)
            continue;
        index += real->auto_allocation ? sizeof(void*) : real->size;
    }
    char* buffer = (char*)malloc(index);
    if(!buffer) return 0;
    memset(buffer, 0, index);
    // Determine the maximum size it can have
    count = 0;
    index = 0;
    real = args->subargs;
    for(; aparse_arg_nend(real); real++)
    {
        if(count == args->layout_size)
            break;
        if(!real->is_argument || !real->is_positional)
            continue;
        // Obviously that is_number only have two state: true/false so dont need if
        real->ptr = &buffer[index];
        real->allocated = true;
        index += real->auto_allocation ? sizeof(void*) : real->size;
        count++;
    }
    return buffer;
}

void aparse_free_composed(char* data, aparse_arg* args)
{
    if(!args)
        return;
    for(aparse_arg* real = args; aparse_arg_nend(real); real++) {
        if(real->auto_allocation && real->flags & BITMASK_0 && real->allocated) {
            char** dst = (char**)(real->ptr);
            free(*dst);
        }
    }
    free(data);
}

// 0 no error, 1 error (just for cleaning up)
int aparse_process_failure(const bool status, const aparse_internal internal, aparse_arg* args)
{
    int error = 0;
    if(status > 0) {
        error = 1;
    }
    else if (internal.positional_count != internal.positional_processed) {
        aparse_required_message(args);
        error = 1;
    }
    else if (internal.unknown.size > 0 && !internal.is_sublayer) {
        aparse_unknown_message(internal);
        error = 1;
    }
    return error;
}


void aparse_required_message(aparse_arg* args)
{
    fprintf(stderr, "%s: error: the following arguments are required: ", progname);

    int printed = 0;
    for (int i = 0; aparse_arg_nend(args); args++) {
        if (args->is_positional && !(args->flags & BITMASK_0)) {
            if (printed > 0)
                fprintf(stderr, ", ");
            fprintf(stderr, "%s", args->longopt);
            printed++;
        }
    }

    fprintf(stderr, "\n");
}

void aparse_unknown_message(const aparse_internal internal)
{
    fprintf(stderr, "%s: error: unrecognized arguments: ", progname);
    int printed = 0;
    for (int i = 0; i < internal.unknown.size; i++) {
        if (printed > 0)
            fprintf(stderr, ", ");
        fprintf(stderr, "%s", (char*)aparse_list_get(&internal.unknown, i));
        printed++;
    }
    fprintf(stderr, "\n");
    // Cleanup
}

void aparse_print_help(aparse_arg* main_args)
{
    printf("positional arguments:\n");
    aparse_print_positional_help(main_args);
    printf("\noptions:\n");
    aparse_print_help_tag(&help_arg, INDENT);
    for(aparse_arg* real = main_args; aparse_arg_nend(real); real++) {
        if(real->is_argument && !real->is_positional)
            aparse_print_help_tag(real, INDENT);
    }
}

void aparse_print_help_tag(const aparse_arg* arg, int indent) {
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

void aparse_print_positional_help(aparse_arg* args) {
    for (; aparse_arg_nend(args); args++) {
        if (args->is_positional) {
            if(args->is_argument) {
                aparse_print_help_tag(args, INDENT);
                continue;
            }
            printf("%*s", INDENT, "");
            aparse_print_available_subcommands(args);
            printf("\n");

            for (aparse_arg* ptr = args->subargs; aparse_arg_nend(ptr); ptr++)
                aparse_print_help_tag(ptr, INDENT * 2);
        }
    }
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

aparse_arg* aparse_argv_matching(const char* argv, aparse_arg* args)
{
    if(!strcmp(argv, help_arg.shortopt) || !strcmp(argv, help_arg.longopt)) {
        return &help_arg;
    }
    for (aparse_arg* real = args; aparse_arg_nend(real); real++) {
        if (real->is_positional) {
            if (!real->flags & BITMASK_0)
                return real;
            continue;
        }
        if (real->shortopt) {
            int shortlen = strlen(real->shortopt);
            if (strncmp(argv, real->shortopt, shortlen) == 0) {
                real->flags |= BITMASK_1;
                if (argv[shortlen] == '\0') {
                    return real;
                } else if (argv[shortlen] == '=') {
                    real->flags |= BITMASK_0;
                    return real;
                }
            }
        }
        if (real->longopt) {
            int longlen = strlen(real->longopt);
            if (strncmp(argv, real->longopt, longlen) == 0) {
                real->flags &= ~BITMASK_1;
                if (argv[longlen] == '\0') {
                    return real;
                } else if (argv[longlen] == '=') {
                    real->flags |= BITMASK_0;
                    return real;
                }
            }
        }
    }
    return NULL;
}