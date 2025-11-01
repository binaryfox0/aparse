/* Priority=0, FileContentStart=25
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

#include "aparse.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <float.h>

#include "aparse_list.h"

#define APARSE_ABORT 1

#define APARSE_ARG_EQUAL_VAL   (1 << 0)
#define APARSE_ARG_SHORT_MATCH (1 << 1)
#define APARSE_ARG_PROCESSED   (1 << 7)

#define INDENT 2 // indent/space
#define MAX_ARG_STR 19

// aparse_arg flags
// optional  has_equal   APARSE_ARG_EQUAL_VAL
// optional  short_match APARSE_ARG_SHORT_MATCH
//
//
//
//
// universal processed   APARSE_ARG_PROCESSED

#define APARSE_LITTLE_ENDIAN (*(unsigned char *)&(uint16_t){1})
#define APARSE_ALLOC_SIZE 5

#define min(a, b) ((a < b) ? (a) : (b))

#ifdef PLATFORM_WIN32
#   include <sdkddkver.h>
// Source:
// - https://learn.microsoft.com/en-us/windows/win32/winprog/using-the-windows-headers?redirectedfrom=MSDN#macros-for-conditional-declarations
// - https://en.wikipedia.org/wiki/ANSI_escape_code#DOS_and_Windows 
#   if defined(NTDDI_VERSION) && (NTDDI_VERSION >= NTDDI_WIN10_TH2)
#       define ANSIES(str) str
#   endif
#else
#   define ANSIES(str) str
#endif

#define aparse_error_label ANSIES("\x1b[1;31m") "error" ANSIES("\x1b[0m") ": "
#define aparse_library_info(fmt, ...) printf("aparse: " ANSIES("\x1b[1;34m") "info" ANSIES("\x1b[0m") ": " fmt "\n", ##__VA_ARGS__)
#define aparse_library_warn(fmt, ...) printf("aparse: " ANSIES("\x1b[1;33m") "warn" ANSIES("\x1b[0m") ": " fmt "\n", ##__VA_ARGS__)
#define aparse_library_error(fmt, ...) fprintf(stderr, "aparse: "  aparse_error_label fmt "\n", ##__VA_ARGS__)

typedef struct aparse_internal
{
    int positional_count;
    int positional_processed;
    aparse_list unknown;
    bool is_sublayer;
} aparse_internal;

typedef struct aparse_help_before // nah idk what to name it.
{
    char* str;
    bool freeable;
} aparse_help_before;

APARSE_INLINE bool aparse_arg_is_positional(const aparse_arg* arg) {
    return arg->type & APARSE_ARG_TYPE_POSITIONAL;
}

APARSE_INLINE bool aparse_arg_is_argument(const aparse_arg* arg) {
    return arg->type & APARSE_ARG_TYPE_ARGUMENT;
}

static inline bool aparse_type_compare(const aparse_arg* arg, const aparse_arg_types type) {
    return (arg->type & APARSE_ARG_TYPE_BITMASK) == type;
}

static inline bool aparse_arg_nend(const aparse_arg* arg) {
    return arg->longopt != 0 || arg->shortopt != 0;
}

static aparse_arg* root_args = 0, *current_args = 0;
static const char* progname = 0;
static const char *prog_desc = 0;
static aparse_arg help_arg = { .shortopt = "-h", .longopt = "--help", .help = "show this help message and exit", .type = APARSE_ARG_TYPE_BOOL };
static bool failure = 1; // only for help message

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
bool aparse_process_optional(int argc, char** argv, int* index, aparse_arg* arg);
// Processing each data type
int aparse_process_float(char* argv, const aparse_arg* arg);
int aparse_process_array(int argc, char** argv, aparse_arg* arg, int* index);
// For aparse_arg is subparsers and have proper data_layout & layout_size
char* aparse_compose_data(const aparse_arg* args);
// Failure handling
int aparse_process_failure(const int status, const aparse_internal internal, aparse_arg* args);
void aparse_required_message(aparse_arg* args);
void aparse_unknown_message(const aparse_internal internal);
// Help-related functions
void aparse_print_help(aparse_arg* main_args);
void aparse_print_help_tag(const aparse_arg* arg, int indent);
void aparse_print_positional_help(aparse_arg* args);
char* aparse_construct_available_subcommands(const aparse_arg* args);
void aparse_print_usage();
// Miscellaneous
aparse_arg* aparse_argv_matching(const char* argv, aparse_arg* args);
const char* aparse_extract_exename(const char* argv0);
char* aparse_construct_optional_argument(char* longopt);
aparse_internal aparse_construct_internal(aparse_arg* args, bool is_sublayer);

void aparse_parse(const int argc, char** argv, aparse_arg* args, const char* program_desc)
{
    progname = aparse_extract_exename(argv[0]);
    prog_desc = program_desc;
    root_args = args;
    aparse_internal internal = aparse_construct_internal(args, false);
    aparse_list_new(&call_list, 0, sizeof(call_struct));
    int index = 1;
    if(aparse_process_failure(aparse_parse_private(argc, argv, args, &index, &internal), internal, args)) {
        aparse_list_free(&internal.unknown);
        exit(failure);
    }
    for(size_t i = 0; i < call_list.size; i++) {
        call_struct* tmp = (call_struct*)aparse_list_get(&call_list, i);
        tmp->args->handler(tmp->data); 
        free(tmp->data);
    }
    aparse_list_free(&call_list);
    aparse_list_free(&internal.unknown);
}

// --------------------------------------- PRIVATE ---------------------------------------
int aparse_parse_private(const int argc, char** argv, aparse_arg* args, int* index, aparse_internal* internal)
{
    current_args = args;
    while (*index < argc) {
        int found = 0;
        char* cargv = argv[*index];
        (*index)++;

        aparse_arg* ptr = aparse_argv_matching(cargv, args);
        if(ptr) {
            if(aparse_arg_is_positional(ptr)) {
                internal->positional_processed++;
                ptr->flags |= APARSE_ARG_PROCESSED;
                if(aparse_arg_is_argument(ptr)) {
                    if(ptr->type & APARSE_ARG_TYPE_ARRAY ?
                        !aparse_process_array(argc, argv, ptr, index) :
                        !aparse_process_argument(cargv, ptr)
                    )
                        return APARSE_ABORT;
                } else  {
                    aparse_process_parser(argc, cargv, argv, ptr, index);
                    current_args = args; // reset it
                }
            } else {
                if(ptr->shortopt != help_arg.shortopt) {
                    if(!aparse_process_optional(argc, argv, index, ptr))
                        return APARSE_ABORT;
                } else {
                    aparse_print_help(args);
                    failure = 0;
                    return APARSE_ABORT;
                }
            }
        } else {
            if (aparse_list_add(&internal->unknown, &cargv)) {
                aparse_library_error("failed to allocate memory for parsing process. Retry again.");
                return APARSE_ABORT;
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

    switch(arg->type & APARSE_ARG_TYPE_BITMASK)
    {
    case APARSE_ARG_TYPE_STRING:
    {
        size_t len = strlen(argv) + 1;

        if (arg->size == 0) { // dynamic
            char** dst = (char**)arg->ptr;
            *dst = argv;
        } else {
            size_t n = min(arg->size - 1, len);
            memcpy(arg->ptr, argv, n);
            ((char*)arg->ptr)[n] = '\0';
        }
        break;
    }
    case APARSE_ARG_TYPE_UNSIGNED:
    {
        bool have_sign = arg->type & APARSE_ARG_TYPE_SIGNED_FLAGS;
        if (arg->size <= 0) {
            aparse_library_error("invalid argument size");
            aparse_library_info("longopt: %s, shortopt: %s", arg->longopt, arg->shortopt);
            return false;
        }

        if(arg->size > sizeof(uint64_t))
        {
            aparse_library_warn("unhandled integer size: %d bytes. Please make this argument a string!", arg->size);
            aparse_library_info("longopt: %s, shortopt: %s", arg->longopt, arg->shortopt);
            return false;
        }

        // Determine base
        int base = 10;
        char* p = argv;
        bool is_negative = false;

        if (have_sign && (*p == '-' || *p == '+')) {
            if (*p == '-') is_negative = true;
            p++;
        }

        if (p[0] == '0') {
            switch(tolower(p[1]))
            {
            case 'x': base = 16; p += 2; break;
            case 'b': base = 2 ; p += 2; break;
            case 'o': base = 8 ; p += 2; break;
            default:  base = 8;  p++; break;
            }
        }

        uint64_t num = 0;
        char* endptr = 0;

        if (have_sign) {
            int64_t signed_val = strtoll(p, &endptr, base);
            num = (uint64_t)signed_val;
        } else {
            num = strtoull(p, &endptr, base);
        }
        
        if(*endptr) {
            fprintf(stderr, "%s: " aparse_error_label "invalid integer '%s'\n", progname, argv);
            return false;
        }

        // Integer overflow check
        size_t bits = arg->size * 8;
        if (have_sign) {
            int64_t min_val = -(1LL << (bits - 1));
            int64_t max_val =  (1LL << (bits - 1)) - 1;
            if ((int64_t)num < min_val || (int64_t)num > max_val) {
                aparse_library_error("value '%s' is too large for argument '%s'",
                                     argv, arg->longopt ? arg->longopt : arg->shortopt);
                return false;
            }
        } else {
            uint64_t max_val = (bits == 64) ? UINT64_MAX : ((1ULL << bits) - 1);
            if (num > max_val) {
                aparse_library_error("value '%s' is too large for argument '%s'",
                                     argv, arg->longopt ? arg->longopt : arg->shortopt);
                return false;
            }
        }

        memcpy(arg->ptr, &num, min(arg->size, 8));

        if (have_sign) {
            uint8_t* b = (uint8_t*)arg->ptr + (APARSE_LITTLE_ENDIAN ? arg->size - 1 : 0);
            *b = (*b & ~0x80) | (((int64_t)num < 0) ? 0x80 : 0);
        }
        break;
    }
    case APARSE_ARG_TYPE_FLOAT:
    {
        int errnum = aparse_process_float(argv, arg);
        switch(errnum) {
        case -1:
            fprintf(stderr, "%s: " aparse_error_label "invalid float '%s'\n", progname, argv);
            return false;
            
        case -2:
            aparse_library_error("value '%s' is too large for argument '%s'", argv, arg->longopt ? arg->longopt : arg->shortopt);
            return false;
        case -3:
            aparse_library_warn("value '%s' underflows precision", argv);
            break;
        case -4:
            aparse_library_warn("unhandled float size: %d bytes. Please make this argument a string!", arg->size);
            aparse_library_info("longopt: %s, shortopt: %s", arg->longopt, arg->shortopt);
            return false;
        }
        break;
    }
    default:
        return false;
    }
    return true;
}


void aparse_process_parser(const int argc, const char* cargv, char** argv, const aparse_arg* arg, int* index)
{
    // Find subcommand for parser
    aparse_arg* ptr2 = arg->subargs;
    int found2;
    for(; aparse_arg_nend(ptr2); ptr2++) {
            if((found2 = !strcmp(cargv, ptr2->longopt)))
                break;
        }
    if(!found2) {
        aparse_print_usage();
        fprintf(stderr, "%s: " aparse_error_label "invalid choice: '%s' (choose from ", progname, cargv);
        for(aparse_arg* ptr2 = arg->subargs; aparse_arg_nend(ptr2); ptr2++) {
            fprintf(stderr, "%s%s", ptr2->longopt, aparse_arg_nend(ptr2 + 1) ? ", " : "");
        }
        fprintf(stderr, ")\n");
        exit(failure);
    }
    // Found it!, compose struct for it to handler process.
    char* data = aparse_compose_data(ptr2);
    aparse_internal internal2 = aparse_construct_internal(ptr2->subargs, true);
    if(aparse_process_failure(aparse_parse_private(argc, argv, 
        ptr2->subargs, index, &internal2), internal2, ptr2->subargs)) {
        free(data);
        aparse_list_free(&internal2.unknown);
        exit(failure);
    }
    if(!ptr2->handler || !data)
        free(data);
    else
        aparse_list_add(&call_list, (call_struct[1]){{ptr2, data}});
    aparse_list_free(&internal2.unknown);
}

bool aparse_process_optional(int argc, char** argv, int* index, aparse_arg* arg){
    if(aparse_type_compare(arg, APARSE_ARG_TYPE_BOOL))
    {
        if(arg->flags & APARSE_ARG_PROCESSED)
            return true;
        bool was_set = false;
        for(int i = 0; i < arg->size; i++)
            if(((char*)arg->ptr)[i])
                was_set = true;
        memset(arg->ptr, 0, arg->size);
        uint8_t* b = (uint8_t*)arg->ptr + (APARSE_LITTLE_ENDIAN ? arg->size - 1 : 0);
        if(!was_set) *b |= 1;
        arg->flags |= APARSE_ARG_PROCESSED;
        return true;
    }
    // if has equal
    if(arg->flags & APARSE_ARG_EQUAL_VAL) {
        const char* optname = (arg->flags & APARSE_ARG_SHORT_MATCH) ? arg->shortopt : arg->longopt;
        return aparse_process_argument(argv[*index - 1] + 1 + strlen(optname), arg);
    }
    else {
        if(*index >= argc) {
            printf("%s: " aparse_error_label "option '%s' expected 1 argument.\n", progname,
                arg->flags & APARSE_ARG_SHORT_MATCH ? arg->shortopt : arg->longopt);
            return false;
        }
        (*index)++;
        return aparse_process_argument(argv[*index - 1], arg);
    }
}

int aparse_process_float(char* argv, const aparse_arg* arg)
{
    char* endptr = 0;
    errno = 0;
    long double num = strtold(argv, &endptr);
    long double abs_val = fabsl(num);
    if(*endptr) {
        return -1;
    }
    if(errno == ERANGE)
    {
        if((num == HUGE_VALL || num == -HUGE_VALL))
            return -2;
        if(num == 0.0 || abs_val < LDBL_MIN)
            return -3;
    }

    switch (arg->size) {
    case sizeof(float):
        if (abs_val > FLT_MAX && abs_val != INFINITY)
            return -2;
        if (num != 0.0L && abs_val < FLT_MIN)
            return -3;
        *(float *)arg->ptr = (float)num;
        break;

    case sizeof(double):
        if (abs_val > DBL_MAX && abs_val != INFINITY)
            return -2;
        if (num != 0.0L && abs_val < DBL_MIN)
            return -3;
        *(double *)arg->ptr = (double)num;
        break;

    case sizeof(long double):
        *(long double *)arg->ptr = num;
        break;

    default:
        return -4;
    }
    return 0;
}

int aparse_process_array(int argc, char** argv, aparse_arg* arg, int* index)
{
    (*index)--;
    int remaining = argc - *index;
    void* new_arr = 0;
    int increment = arg->type & APARSE_ARG_TYPE_STRING ? sizeof(char*) : arg->element_size;

    if(arg->size == 0)
    {
        new_arr = malloc(increment * remaining);
        if(!new_arr) {
            aparse_library_error("failed to allocate memory for parsing process. Retry again.");
            return 0;
        }
        if(arg->ptr)
            *(void**)arg->ptr = new_arr; // If user specified a ptr->ptr, assign new ptr
        arg->ptr = new_arr; // Override old ptr
        arg->size = remaining;
    } else {
        if(remaining < arg->size) {
            fprintf(stderr, "%s: " aparse_error_label "'%s' expected %d arguments, but only %d was provided.\n",
                progname, arg->longopt, arg->size, remaining);
            return 0;
        }
    }
    if(increment <= 0) {
        aparse_library_error("invalid element size (%d) for array paramater.", arg->element_size);
        return 0;
    }
    int array_size = arg->size;
    arg->size = arg->element_size;
    for(int i = 0; i < array_size; i++, (*index)++, arg->ptr += increment)
        if(!aparse_process_argument(argv[(*index)], arg))
            return 0;
    arg->size = array_size;
    return 1;
}

// Use to create a bytes-like structure to hold data before parsing then pass into handler
char* aparse_compose_data(const aparse_arg* args)
{
    if(!args->subargs || !args->handler)
        return 0;

    if(args->size < 1) return 0;
    // Find the structure size using the final member size and offset
    size_t size = args->data_layout[(args->size - 1) * 2 + 0] + args->data_layout[(args->size - 1) * 2 + 1];
    char* buffer = malloc(size);
    if(!buffer) return 0;
    memset(buffer, 0, size);
    aparse_arg* real = args->subargs;
    int count = 0;
    for(int i = 0; i < args->size && aparse_arg_nend(real); i++, real++)
    {
        if(!aparse_arg_is_argument(real) || !aparse_arg_is_positional(real))
            continue;
        int base = i * 2;
        real->ptr = &buffer[args->data_layout[base]];
        count++;
        if(real->size == 0 && aparse_type_compare(real, APARSE_ARG_TYPE_STRING))
            continue;
        real->size = args->data_layout[base + 1];
    }
    if(!count) {
        free(buffer);
        return 0;
    }
    return buffer;
}

// 0 no error, 1 error (just for cleaning up)
int aparse_process_failure(const int status, const aparse_internal internal, aparse_arg* args) {
    if (status == APARSE_ABORT) return APARSE_ABORT;

    // Not using != here as if an array paramater was specified, the positional_count have already been -= 1
    if (internal.positional_count > internal.positional_processed) {
        aparse_print_usage();
        aparse_required_message(args);
        return APARSE_ABORT;
    }

    if (internal.unknown.size > 0 && !internal.is_sublayer) {
        aparse_unknown_message(internal);
        return APARSE_ABORT;
    }

    return 0;
}

void aparse_required_message(aparse_arg* args) {
    fprintf(stderr, "%s: " aparse_error_label "the following arguments are required: ", progname);
    int printed = 0;
    for (; aparse_arg_nend(args); args++) {
        if (aparse_arg_is_positional(args) && !(args->flags & APARSE_ARG_PROCESSED)) {
            if (printed++ > 0) fprintf(stderr, ", ");
            fprintf(stderr, "%s", args->longopt);
        }
    }
    fprintf(stderr, "\n");
}

void aparse_unknown_message(const aparse_internal internal) {
    fprintf(stderr, "%s: " aparse_error_label "unrecognized arguments: ", progname);
    for (int i = 0; i < internal.unknown.size; i++) {
        if (i > 0) fprintf(stderr, ", ");
        fprintf(stderr, "%s", *(char**)aparse_list_get(&internal.unknown, i));
    }
    fprintf(stderr, "\n");
}

void aparse_print_help(aparse_arg* main_args) {
    aparse_print_usage();
    printf("\n");
    if(root_args == current_args) {
        if(prog_desc)
            printf("%s\n\n", prog_desc);
    }
    printf("positional arguments:\n");
    aparse_print_positional_help(main_args);
    printf("\noptions:\n");
    aparse_print_help_tag(&help_arg, INDENT);

    for (aparse_arg* real = main_args; aparse_arg_nend(real); real++) {
        if (aparse_arg_is_argument(real) && !aparse_arg_is_positional(real))
            aparse_print_help_tag(real, INDENT);
    }
}

void aparse_print_help_tag(const aparse_arg* arg, int indent) {
    if(!aparse_arg_is_argument(arg) && !arg->help && !aparse_arg_nend(arg)) return;
    int len = printf("%*s%s%s%s", 
        indent, "", 
        arg->shortopt ? arg->shortopt : "", 
        arg->shortopt ? ", " : "",
        arg->longopt  ? arg->longopt  : ""
    ) - INDENT;
    if(!aparse_arg_is_positional(arg) && arg->type != APARSE_ARG_TYPE_BOOL) {
        char* optional_arg = aparse_construct_optional_argument(arg->longopt ? arg->longopt : arg->shortopt);
        len += printf(" %s", optional_arg);
        free(optional_arg);
    }
    bool longer = len > MAX_ARG_STR;
    if(arg->help) {
        int space = INDENT + (longer ? INDENT + MAX_ARG_STR : MAX_ARG_STR - len);
        printf("%s%*s%s", longer ? "\n" : "", space, "", arg->help);
    }
    printf("\n");
}

void aparse_print_positional_help(aparse_arg* args) {
    for (; aparse_arg_nend(args); args++) {
        if (aparse_arg_is_positional(args)) {
            if(aparse_arg_is_argument(args)) {
                aparse_print_help_tag(args, INDENT);
                continue;
            }
            printf("%*s", INDENT, "");
            char* buf = aparse_construct_available_subcommands(args);
            printf("%s\n", buf);
            if(buf) free(buf);

            for (aparse_arg* ptr = args->subargs; aparse_arg_nend(ptr); ptr++)
                aparse_print_help_tag(ptr, INDENT * 2);
        }
    }
}

char* aparse_construct_available_subcommands(const aparse_arg* args) {
    if (!args || !args->subargs)
        return strdup("{}");

    size_t buffer_size = 16;
    char* buffer = malloc(buffer_size);
    if (!buffer) return NULL;

    size_t len = 0;
    len += snprintf(buffer + len, buffer_size - len, "{");

    int index = 0;
    for (const aparse_arg* ptr = args->subargs; aparse_arg_nend(ptr); ptr++) {
        const char* name = ptr->longopt ? ptr->longopt : "";
        size_t needed = strlen(name) + 3; // ", " or closing "}"
        if (len + needed >= buffer_size) {
            buffer_size *= 2;
            char* new_buffer = realloc(buffer, buffer_size);
            if (!new_buffer) {
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
        }

        if (index++ > 0)
            len += snprintf(buffer + len, buffer_size - len, ", ");
        len += snprintf(buffer + len, buffer_size - len, "%s", name);
    }

    if (len + 2 >= buffer_size) {
        char* new_buffer = realloc(buffer, buffer_size + 2);
        if (!new_buffer) {
            free(buffer);
            return NULL;
        }
        buffer = new_buffer;
    }
    snprintf(buffer + len, buffer_size - len, "}");

    return buffer;
}
bool aparse_print_usage_before_r(aparse_arg* args, aparse_arg* target, aparse_list* strs) {
    if (args == target) {
        return true;
    }
    for (; aparse_arg_nend(args); args++) {
        if (!aparse_arg_is_positional(args)) continue;

        if (aparse_arg_is_argument(args)) {
            aparse_help_before entry = { args->longopt, false };
            aparse_list_add(strs, &entry);
        } else if (args->subargs) {
            int start_index = strs->size;

            for (aparse_arg* sub = args->subargs; aparse_arg_nend(sub); sub++) {
                aparse_help_before entry = { sub->longopt, false };
                aparse_list_add(strs, &entry);
                if (sub->subargs && aparse_print_usage_before_r(sub->subargs, target, strs)) {
                    return true;
                }
                strs->size = start_index;  // Rollback
            }

            char* alt = aparse_construct_available_subcommands(args);
            if (alt) {
                aparse_help_before entry = { alt, true };
                aparse_list_add(strs, &entry);
            }
        }
    }
    return false;
}

void aparse_print_usage_before(aparse_arg* root, aparse_arg* target) {
    aparse_list strs;
    aparse_list_new(&strs, 0, sizeof(aparse_help_before));

    aparse_print_usage_before_r(root, target, &strs);

    for (size_t i = 0; i < strs.size; i++) {
        aparse_help_before* entry = aparse_list_get(&strs, i);
        printf("%s ", entry->str);
        if (entry->freeable) free(entry->str);
    }

    aparse_list_free(&strs);
}

void aparse_print_usage_after(aparse_arg* args)
{
    aparse_list list;
    aparse_list_new(&list, 0, sizeof(aparse_arg*));
    for(aparse_arg* real = args; aparse_arg_nend(real); real++)
    {
        if(aparse_arg_is_positional(real))
            aparse_list_add(&list, &real);
        else {
            printf("[%s", real->shortopt ? real->shortopt : real->longopt);
            if(!aparse_type_compare(real, APARSE_ARG_TYPE_BOOL))
            {
                char* optional_arg = aparse_construct_optional_argument(real->longopt ? real->longopt : real->shortopt);
                printf(" %s", optional_arg);
                free(optional_arg);
            }
            printf("] ");
        }
    }
    for(size_t i = 0; i < list.size; i++)
    {
        aparse_arg* entry = *(aparse_arg**)aparse_list_get(&list, i);
        if(aparse_arg_is_argument(entry))
            printf("%s ", entry->longopt);
        else {
            char* buf = aparse_construct_available_subcommands(entry);
            printf("%s ... ", buf);
            if(buf) free(buf);
        }
    }
    aparse_list_free(&list);
    printf("\n");
}

void aparse_print_usage() {
    printf("usage: %s ", progname);
    aparse_print_usage_before(root_args, current_args);
    aparse_print_usage_after(current_args);
}

aparse_arg* aparse_argv_matching(const char* argv, aparse_arg* args)
{
    if(!strcmp(argv, help_arg.shortopt) || !strcmp(argv, help_arg.longopt)) {
        return &help_arg;
    }
    aparse_arg* positional = 0;
    for (aparse_arg* real = args; aparse_arg_nend(real); real++) {
        if (aparse_arg_is_positional(real)) {
            if (!(real->flags & APARSE_ARG_PROCESSED))
                if(!positional)
                    positional = real;
            continue;
        }
        if (real->shortopt) {
            int shortlen = strlen(real->shortopt);
            if (strncmp(argv, real->shortopt, shortlen) == 0) {
                real->flags |= APARSE_ARG_SHORT_MATCH;
                if (argv[shortlen] == '\0') {
                    return real;
                } else if (argv[shortlen] == '=') {
                    real->flags |= APARSE_ARG_EQUAL_VAL;
                    return real;
                }
            }
        }
        if (real->longopt) {
            int longlen = strlen(real->longopt);
            if (strncmp(argv, real->longopt, longlen) == 0) {
                real->flags &= ~APARSE_ARG_SHORT_MATCH;
                if (argv[longlen] == '\0') {
                    return real;
                } else if (argv[longlen] == '=') {
                    real->flags |= APARSE_ARG_EQUAL_VAL;
                    return real;
                }
            }
        }
    }
    return positional;
}

const char* aparse_extract_exename(const char* argv0)
{
    const char* last_slash = strrchr(argv0, '/');  // POSIX (Linux, macOS)
#ifdef _WIN32
    const char* last_backslash = strrchr(argv0, '\\');  // Windows
    if (!last_slash || (last_backslash && last_backslash > last_slash)) {
        last_slash = last_backslash;
    }
#endif
    return last_slash ? last_slash + 1 : argv0;
}

char* aparse_construct_optional_argument(char* longopt)
{
    if(!longopt)
        return 0;
    for(; *longopt != '\0' && *longopt == '-'; longopt++) {}
    char* out, *ptr;
    out = ptr = malloc(strlen(longopt) + 1);
    if(!out) return 0;
    for(; *longopt != '\0'; longopt++, ptr++)
        *ptr = toupper(*longopt);
    *ptr = '\0';
    return out;
}

aparse_internal aparse_construct_internal(aparse_arg* args, bool is_sublayer)
{
    int positional_count = 0;
    for(; aparse_arg_nend(args); args++) {
        if(aparse_arg_is_positional(args))
            positional_count++;
        if(args->type & APARSE_ARG_TYPE_ARRAY && args->size == 0)
            positional_count--;
    }
    // aparse_library_info("%d", positional_count);
    aparse_internal out = { positional_count, 0, {0, 0, 0}, is_sublayer};
    aparse_list_new(&out.unknown, 0, sizeof(char*));
    return out;
}