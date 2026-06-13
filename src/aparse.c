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

#ifdef _WIN32
#   include <windows.h>
#else
#   include <unistd.h>
#   include <sys/ioctl.h>
#endif

#define APARSE_ARG_EQUAL_VAL   (1 << 0)
#define APARSE_ARG_SHORT_MATCH (1 << 1)
#define APARSE_ARG_PROCESSED   (1 << 7)

#define INDENT 2 // indent/space
#define MAX_ARG_STR 19

// aparse_arg flags
// optional  | has_equal   APARSE_ARG_EQUAL_VAL
// optional  | short_match APARSE_ARG_SHORT_MATCH
// reserved  | 
// reserved  | 
// reserved  | 
// reserved  | 
// universal | processed   APARSE_ARG_PROCESSED

#define APARSE_LITTLE_ENDIAN (*(unsigned char *)&(uint16_t){1})
#define APARSE_ALLOC_SIZE 5

#define min(a, b) ((a < b) ? (a) : (b))

#define aparse_library_debug(fmt, ...) \
    __aparse_fprintf(stderr, "aparse: " __aparse_debug_label ": " fmt "\n", ##__VA_ARGS__)
#define aparse_library_info(fmt, ...) \
    __aparse_fprintf(stderr, "aparse: " __aparse_info_label  ": " fmt "\n", ##__VA_ARGS__)
#define aparse_library_warn(fmt, ...) \
    __aparse_fprintf(stderr, "aparse: " __aparse_warn_label  ": " fmt "\n", ##__VA_ARGS__)
#define aparse_library_error(fmt, ...) \
    __aparse_fprintf(stderr, "aparse: " __aparse_error_label ": "fmt "\n", ##__VA_ARGS__)
#define aparse_raise_error(type, field1, field2) \
    { err_cb((type), (field1), (field2), err_userdata); return APARSE_STATUS_FAILURE; }

#define aparse__foreach(child, parent) \
    for(aparse_arg *child = parent->subargs; aparse_arg_nend(child); child++)

// The command handler for subparser will be called after the main parsing completed
typedef struct aparse_dispatch {
    aparse_arg* args;
    void* data;
} aparse__dispatch_entry_t;

typedef struct aparse_context
{
    int *idx_ptr;
    int positional_count;
    int positional_processed;
    aparse_list *unknown;
    aparse_list *dispatch;
    int layer_idx;
} aparse__context_t;

typedef struct aparse_help_before // nah idk what to name it.
{
    const char* str;
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

static aparse_arg* root_args = 0, *current_args = 0;
const char* __aparse_progname = 0;
static const char *prog_desc = 0;
static aparse_arg help_arg = { .shortopt = "-h", .longopt = "--help", .help = "show this help message and exit", .type = APARSE_ARG_TYPE_BOOL };
static bool failure = 1; // only for help message
static aparse_error_callback err_cb = 0;
static void* err_userdata = 0;

// Forward declaration
static int aparse__parse_impl(
        const int argc, 
        char * const * argv, 
        aparse_arg* args, 
        aparse__context_t* ctx
);

// Processing each type of argument
static int aparse__process_argument(
        const char* argv, 
        const aparse_arg* arg,
        aparse__context_t *ctx);

static int aparse__process_parser(
        const int argc, 
        const char* cargv, 
        char* const* argv, 
        aparse_arg* arg, 
        aparse__context_t* context
);

static int aparse_process_optional(
        const int argc, 
        char* const* argv, 
        aparse_arg* arg,
        aparse__context_t *ctx
);
// Processing each data type
static int aparse_process_float(
        const char* argv, 
        const aparse_arg* arg);

static int aparse_process_array(
        const int argc, 
        char* const* argv, 
        aparse_arg* arg, 
        aparse__context_t *ctx);

// For aparse_arg is subparsers and have proper data_layout & layout_size
static int aparse_evaluate_size(
        const aparse_arg* arg);

static int aparse_verify_layout(
        const aparse_arg *arg, 
        int *invalid_idx);

static int aparse_fill_args_dest(
        const aparse_arg* arg, 
        uint8_t *buffer);

static void aparse_destroy_data(
        const aparse_arg* args, 
        char* composed);
// Failure handling
static int aparse_process_failure(
        const int status, 
        const aparse__context_t *context, aparse_arg* args);

static void aparse_default_error_callback(
        const aparse_status status, 
        const void* field1, 
        const void* field2, 
        void* userdata);

// Help-related functions
static void aparse__print_help(
        aparse_arg* main_args,
        aparse__context_t *ctx);

static void aparse__print_wrapped(
        const char *text, 
        int start_col, 
        int term_width);

static void aparse_print_help_tag(
        const aparse_arg* arg, 
        const int indent
);

static void aparse_print_positional_help(aparse_arg* args);
static char* aparse_construct_available_subcommands(const aparse_arg* args);
static void aparse_print_usage(void);
// Miscellaneous
static aparse_arg* aparse_argv_matching(const char* argv, aparse_arg* args);
static const char* aparse_extract_exename(const char* argv0);
static size_t aparse__option_value_index(const char* opt);
static void aparse__context_init(
        aparse__context_t *ctx,
        const aparse__context_t *prev, 
        aparse_arg* args);

static int aparse__count_positional(
        aparse_arg *args);

static int aparse_get_terminal_width(void);


int aparse_parse(
        const int argc, 
        char* const * argv,
        aparse_arg* args, 
        aparse_list* dispatch_list_out, 
        const char* program_desc)
{
    int ret = APARSE_STATUS_OK;
    aparse__context_t ctx = {0};
    int idx = 1;
    aparse_list unknown_list = {0}; // char*
    aparse_list dispatch_list = {0}; // aparse__dispatch_entry_t*

    if(!argv)
        return APARSE_STATUS_FAILURE;
    __aparse_progname = aparse_extract_exename(argv[0]);
    prog_desc = program_desc;
    root_args = args;

    if(!args)
        return APARSE_STATUS_OK;

    unknown_list.itemsz = sizeof(const char*);
    dispatch_list.itemsz = sizeof(aparse__dispatch_entry_t);

    ctx.idx_ptr = &idx;
    ctx.unknown = &unknown_list;
    ctx.dispatch = &dispatch_list;
    ctx.positional_count = aparse__count_positional(args);
    
    if(!err_cb)
        aparse_set_error_callback(0, 0);

    ret = aparse_process_failure(
                aparse__parse_impl(argc, argv, args, &ctx), 
                &ctx, args);
    
    if(ret == APARSE_STATUS_OK && unknown_list.size > 0)
    {
        err_cb(APARSE_STATUS_UNKNOWN_ARGUMENT, &unknown_list, 0, err_userdata);
        ret = APARSE_STATUS_FAILURE;
    }
    aparse_list_free(&unknown_list);

    if(ret == APARSE_STATUS_OK)
    {
        if(dispatch_list_out)
            *dispatch_list_out = dispatch_list;
        else
            aparse_dispatch_all(&dispatch_list);
    }

    return ret;
}

void aparse_dispatch_all(
        aparse_list* dispatch_list)
{
    if(!dispatch_list || !dispatch_list->ptr || dispatch_list->size < 1)
        return;
    if(dispatch_list->itemsz != sizeof(aparse__dispatch_entry_t))
        return;
    aparse__dispatch_entry_t* list = dispatch_list->ptr;
    for(size_t i = 0; i < dispatch_list->size; i++)
    {
        list[i].args->handler(list[i].data);
        aparse_destroy_data(list[i].args, list[i].data);
    }
    aparse_list_free(dispatch_list);
}

int aparse_dispatch_contain(
        const aparse_list* dispatch_list, 
        const char* name)
{
    if(
            !dispatch_list || 
            !dispatch_list->ptr || 
            dispatch_list->size < 1 || 
            !name || 
            !*name
    ) return 0;
    aparse__dispatch_entry_t* list = dispatch_list->ptr;
    for(size_t i = 0; i < dispatch_list->size; i++)
    {
        if(!list[i].args->longopt)
            continue;
        if(!strcmp(list[i].args->longopt, name))
            return 0;
    }
    return 1;
}

void aparse_dispatch_free(
        aparse_list* dispatch_list)
{
    if(!dispatch_list || !dispatch_list->ptr || dispatch_list->size < 1)
        return;
    aparse__dispatch_entry_t* list = dispatch_list->ptr;
    for(size_t i = 0; i < dispatch_list->size; i++)
        if(list[i].data)
            free(list[i].data);
}

void aparse_set_error_callback(const aparse_error_callback cb, void* userdata)
{
    if(!cb)
        err_cb = aparse_default_error_callback, err_userdata = 0;
    else
        err_cb = cb, err_userdata = userdata;
}

const char* aparse_error_msg(const aparse_status status)
{
    static const char* error_msg[] = 
    {
        [APARSE_STATUS_OK]                  = "Parsing succeeded with no errors.",
        [APARSE_STATUS_FAILURE]             = "General parsing failure (unspecified).",
        [APARSE_STATUS_UNKNOWN_ARGUMENT]    = "Unrecognized command-line argument or option.",
        [APARSE_STATUS_MISSING_VALUE]       = "Options/Arrays expected multiple value, but none was provided.",
        [APARSE_STATUS_INVALID_VALUE]       = "Value provided for the argument was invalid.",
        [APARSE_STATUS_OVERFLOW]            = "Numeric value exceeds supported range.",
        [APARSE_STATUS_UNDERFLOW]           = "Numeric value is below supported range.",
        [APARSE_STATUS_MISSING_POSITIONAL]  = "Expected positional argument was not provided.",
        [APARSE_STATUS_INVALID_SUBCOMMAND]  = "Subcommand not found or unrecognized.",
        [APARSE_STATUS_NULL_POINTER]        = "A required pointer argument was NULL.",
        [APARSE_STATUS_INVALID_TYPE]        = "Argument type is invalid or mismatched.",
        [APARSE_STATUS_INVALID_SIZE]        = "Argument size is invalid for its type.",
        [APARSE_STATUS_INVALID_LAYOUT]      = "The given data layout for subcommand is invalid",
        [APARSE_STATUS_ALLOC_FAILURE]       = "Memory allocation failed.",
        [APARSE_STATUS_UNHANDLED]           = "Unhandled type of argument."
    };
    if(status < 0 && status >= __APARSE_STATUS_ENUM_END__)
        return 0;
    return error_msg[status];
}

// --------------------------------------- PRIVATE ---------------------------------------
static int aparse__parse_impl(
        const int argc, 
        char* const * argv, 
        aparse_arg* args, 
        aparse__context_t* ctx)
{
    int *idx = ctx->idx_ptr;
    if(!args)
        return APARSE_STATUS_OK;
    current_args = args;
    while (*idx < argc) {
        int found = 0;
        const char* cargv = argv[*idx];
        (*idx)++;

        aparse_arg* ptr = aparse_argv_matching(cargv, args);
        if(ptr) {
            if(aparse_arg_is_positional(ptr)) {
                ctx->positional_processed++;
                ptr->flags |= APARSE_ARG_PROCESSED;
                if(aparse_arg_is_argument(ptr)) 
                {
                    aparse_status status = APARSE_STATUS_OK;
                    
                    if(ptr->type & APARSE_ARG_TYPE_ARRAY)
                        status = aparse_process_array(
                                argc, 
                                argv, 
                                ptr, 
                                ctx);
                    else 
                        status = aparse__process_argument(
                                cargv, 
                                ptr, 
                                ctx);
                    if(status != APARSE_STATUS_OK)
                        return APARSE_STATUS_FAILURE;
                } else  {
                    if(aparse__process_parser(argc, cargv, argv, ptr, 
                                ctx) != APARSE_STATUS_OK)
                        return APARSE_STATUS_FAILURE;
                    current_args = args; // reset it
                }
            } else {
                if(ptr->shortopt != help_arg.shortopt) {
                    if(aparse_process_optional(
                                argc, 
                                argv, 
                                ptr, 
                                ctx) != APARSE_STATUS_OK)
                        return APARSE_STATUS_FAILURE;
                } else {
                    aparse__print_help(args, ctx);
                    failure = 0; // It's not failure
                    return APARSE_STATUS_FAILURE;
                }
            }
        } else {
            if (ctx->layer_idx > 0) {
                // allow option to be processed again
                (*idx)--; 
                return -1;
            }

            if (!aparse_list_add(ctx->unknown, &cargv)) 
            {
                err_cb(APARSE_STATUS_ALLOC_FAILURE, 
                        0, 0, err_userdata);
                return APARSE_STATUS_FAILURE;
            }
        }
    }

    return 0;
}

static int aparse__process_argument(
        const char* argv, 
        const aparse_arg *arg,
        aparse__context_t *ctx) 
{
    if (!arg->ptr)
    {
        err_cb(APARSE_STATUS_NULL_POINTER, arg, 0, err_userdata);
        return APARSE_STATUS_OK; // continue
    }
    if (arg->size <= 0 && !(arg->type & APARSE_ARG_TYPE_STRING))
        aparse_raise_error(APARSE_STATUS_INVALID_SIZE, arg, &arg->size);

    switch(arg->type & APARSE_ARG_TYPE_BITMASK)
    {
    case APARSE_ARG_TYPE_STRING:
    {
        size_t len = strlen(argv) + 1;

        if (arg->size == 0) { // dynamic
            const char** dst = (const char**)arg->ptr;
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
        if(arg->size > sizeof(uint64_t))
            aparse_raise_error(APARSE_STATUS_UNHANDLED, arg, 0);

        // Determine base
        int base = 10;
        const char* p = argv;
        bool is_negative = false;

        if (*p == '-' || *p == '+') {
            if (*p == '-') is_negative = true;
            p++;
        }
        if(is_negative && !have_sign)
            aparse_raise_error(APARSE_STATUS_UNDERFLOW, arg, 0);

        if (p[0] == '0') {
            switch(tolower(p[1]))
            {
                case 'x': base = 16; p += 2; break;
                case 'b': base = 2 ; p += 2; break;
                case 'o': base = 8 ; p += 2; break;
                default:  base = 8;  p++; break;
            }
        }

        errno = 0;
        char* endptr = 0;
        uint64_t num = strtoull(p, &endptr, base);
        
        if(*endptr)
            aparse_raise_error(APARSE_STATUS_INVALID_VALUE, arg, argv);
 
        // Overflow/Underflow check No.1 (for 8 bytes)
        if(errno == ERANGE)
        {
            if(num == ULLONG_MAX || (int64_t)num == LLONG_MAX)
                aparse_raise_error(APARSE_STATUS_OVERFLOW, arg, argv);
            if((int64_t)num == LLONG_MIN)
                aparse_raise_error(APARSE_STATUS_UNDERFLOW, arg, argv);
        } else {
            // Overflow/Underflow check No.2
            size_t bits = arg->size * 8;
            if (have_sign) {
                if(is_negative)
                {
                    if(num > (uint64_t)INT64_MAX + 1ULL)
                        aparse_raise_error(APARSE_STATUS_UNDERFLOW, arg, argv);
                    if(num == (uint64_t)INT64_MAX + 1ULL)
                        num = (uint64_t)INT64_MIN;
                    else
                        num = (uint64_t) -(int64_t)num;
                } else {
                    if(num > (uint64_t)INT64_MAX)
                        aparse_raise_error(APARSE_STATUS_OVERFLOW, arg, argv);
                int64_t min_val = bits == 64 ? INT64_MIN : -(1LL << (bits - 1));
                int64_t max_val = bits == 64 ? INT64_MAX :  (1LL << (bits - 1)) - 1;
                if ((int64_t)num < min_val || (int64_t)num > max_val)
                    aparse_raise_error(APARSE_STATUS_OVERFLOW, arg, argv);
                }
            } else {
                uint64_t max_val = bits == 64 ? UINT_MAX : ((1ULL << bits) - 1);
                if (num > max_val)
                    aparse_raise_error(APARSE_STATUS_OVERFLOW, arg, argv);
            }
        }

        memcpy(arg->ptr, (uint8_t*)&num + (APARSE_LITTLE_ENDIAN ? 0 : 8 - arg->size), arg->size);

        if (have_sign) {
            uint8_t* msb = (uint8_t*)arg->ptr + (APARSE_LITTLE_ENDIAN ? arg->size - 1 : 0);
            *msb = (*msb & ~0x80) | (((int64_t)num < 0) ? 0x80 : 0);
        }
        break;
    }
    case APARSE_ARG_TYPE_FLOAT:
    {
        int errnum = aparse_process_float(argv, arg);
        switch(errnum) {
        case -1:
            aparse_raise_error(APARSE_STATUS_INVALID_VALUE, arg, argv);
        case -2:
            aparse_raise_error(APARSE_STATUS_OVERFLOW, arg, argv);
        case -3:
            err_cb(APARSE_STATUS_UNDERFLOW, arg, argv, err_userdata);
            break;
        case -4:
            aparse_raise_error(APARSE_STATUS_UNHANDLED, arg, 0);
        }
        break;
    }
    default:
        aparse_raise_error(APARSE_STATUS_INVALID_TYPE, arg, 0);
    }
    return APARSE_STATUS_OK;
}


int aparse__process_parser(
        const int argc,
        const char* cargv,
        char* const* argv,
        aparse_arg* arg,
        aparse__context_t* ctx)
{
    aparse_arg *ptr2 = 0;
    int found2 = 0;
    uint8_t* buffer = 0;
    int invalid_idx = 0;
    int min_size = 0;
    aparse__context_t new_ctx = {0};

    if(!arg->subargs)
    {
        err_cb(APARSE_STATUS_NULL_POINTER, 
                arg, 0, err_userdata);
        return APARSE_STATUS_OK;
    }
    
    ptr2 = arg->subargs;
    for(; aparse_arg_nend(ptr2); ptr2++) {
            if((found2 = !strcmp(cargv, ptr2->longopt)))
                break;
        }
    if(!found2) {
        aparse_list arg_list = { .ptr = (void*)arg->subargs };
        for(aparse_arg* copy = arg_list.ptr; aparse_arg_nend(copy); copy++)
            arg_list.size++;
        aparse_raise_error(APARSE_STATUS_INVALID_SUBCOMMAND, 
                &arg_list, cargv);
    }

    if(!ptr2->subargs)
    {
        aparse_list_add(ctx->dispatch, 
                (aparse__dispatch_entry_t[1]){{ptr2, 0}});
        return APARSE_STATUS_OK;
    }

    if(!aparse_verify_layout(ptr2, &invalid_idx))
        aparse_raise_error(APARSE_STATUS_INVALID_LAYOUT, ptr2, &invalid_idx);
    min_size =
            ptr2->data_layout[(ptr2->layout_size - 1) * 2] + 
            ptr2->data_layout[(ptr2->layout_size - 1) * 2 + 1];
    if(!ptr2->ptr)
    {
        buffer = calloc(min_size, sizeof(*buffer));
        if(!buffer)
            aparse_raise_error(APARSE_STATUS_ALLOC_FAILURE, 0, 0);
    } else {
        if(ptr2->size < min_size)
            aparse_raise_error(APARSE_STATUS_INVALID_SIZE, ptr2, &ptr2->size);
        buffer = ptr2->ptr;
    }

    aparse_fill_args_dest(ptr2, buffer);

    aparse__context_init(&new_ctx, ctx, 
            ptr2->subargs);
    if(aparse_process_failure(aparse__parse_impl(argc, argv, 
        ptr2->subargs, &new_ctx), 
                &new_ctx, ptr2->subargs)) 
    {
        free(buffer);
        return APARSE_STATUS_FAILURE;
    }
    if(!ptr2->handler)
        free(buffer);
    else
        aparse_list_add(ctx->dispatch, (aparse__dispatch_entry_t[1]){{ptr2, buffer}});
    return APARSE_STATUS_OK;
}

int aparse_process_optional(
        const int argc, 
        char* const* argv, 
        aparse_arg* arg,
        aparse__context_t *ctx)
{
    int *idx = ctx->idx_ptr;
    if(aparse_type_compare(arg, APARSE_ARG_TYPE_BOOL))
    {
        if(arg->flags & APARSE_ARG_PROCESSED)
            return APARSE_STATUS_FAILURE;
        bool was_set = false;
        for(int i = 0; i < arg->size; i++)
            if(((char*)arg->ptr)[i])
                was_set = true;
        memset(arg->ptr, 0, arg->size);
        uint8_t* b = (uint8_t*)arg->ptr + (APARSE_LITTLE_ENDIAN ? 0 : arg->size - 1);
        if(!was_set) *b |= 1;
        arg->flags |= APARSE_ARG_PROCESSED;
        return APARSE_STATUS_OK;
    }
    // if has equal
    if(arg->flags & APARSE_ARG_EQUAL_VAL) {
        const char* optname = (arg->flags & APARSE_ARG_SHORT_MATCH) ? arg->shortopt : arg->longopt;
        return aparse__process_argument(
                argv[*idx - 1] + 1 + strlen(optname), 
                arg,
                ctx);
    }
    else {
        if(*idx >= argc) {
            int expected_count = 1;
            aparse_raise_error(APARSE_STATUS_MISSING_VALUE, arg, &expected_count);
        }
        (*idx)++;
        return aparse__process_argument(
                argv[*idx - 1], 
                arg, 
                ctx);
    }
}

int aparse_process_float(const char* argv, const aparse_arg* arg)
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

    switch (arg->size) 
    {
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

static int aparse_process_array(
        const int argc, 
        char* const * argv, 
        aparse_arg* arg, 
        aparse__context_t *ctx)
{
    int *idx = ctx->idx_ptr;
    aparse_list* dest = arg->ptr;
    int remaining = 0, increment = 0;
    void *ptr = 0;

    if(!dest)
        aparse_raise_error(APARSE_STATUS_NULL_POINTER, arg, 0);
    if(arg->size < sizeof(aparse_list))
        aparse_raise_error(APARSE_STATUS_INVALID_SIZE, arg, &arg->size);
    if((arg->type & APARSE_ARG_TYPE_BITMASK) == APARSE_ARG_TYPE_UNKNOWN)
        aparse_raise_error(APARSE_STATUS_INVALID_TYPE, arg, 0);

    (*idx)--;
    remaining = argc - *idx;
    increment = arg->type & APARSE_ARG_TYPE_STRING ? 
        sizeof(char*) : 
        arg->element_size;
    if(increment <= 0)
        aparse_raise_error(APARSE_STATUS_INVALID_SIZE, arg, &arg->element_size);

    int arrsz = remaining;
    if(arrsz < arg->array_size)
        aparse_raise_error(APARSE_STATUS_MISSING_VALUE, arg, &remaining);
    
    ptr = malloc(increment * arrsz);
    if(!ptr)
        aparse_raise_error(APARSE_STATUS_ALLOC_FAILURE, 0, 0);
    dest->capacity = arrsz;
    dest->ptr = ptr;
    dest->itemsz = increment;
    arg->ptr = ptr;
    arg->size = arg->element_size;
    while(dest->size < dest->capacity)
    {
        if(aparse__process_argument(
                    argv[(*idx)], 
                    arg,
                    ctx) != APARSE_STATUS_OK)
            return APARSE_STATUS_FAILURE;
        dest->size++;
        (*idx)++; 
        arg->ptr += increment;
    }
    // re-assign to prevent SIGSEGV in aparse_destroy_data
    arg->ptr = dest;
    return APARSE_STATUS_OK;
}

// Use to evaluate the size of given argument, therefore checking it with data layout
static int aparse_evaluate_size(const aparse_arg* arg)
{
    if(arg->type & APARSE_ARG_TYPE_ARRAY)
        return sizeof(aparse_list);
    if(arg->size == 0 && aparse_type_compare(arg, APARSE_ARG_TYPE_STRING))
        return sizeof(char*);
    return arg->size;
}

static int aparse_verify_layout(
        const aparse_arg *arg,
        int *invalid_idx)
{
    int prev_offset = -1;
    int prev_size = 0;

    const int *layout = arg->data_layout;
    int layout_size = arg->layout_size;

    const aparse_arg *arg_ptr = arg->subargs;

    if (!arg->subargs && !layout && layout_size == 0)
        return APARSE_STATUS_OK;

    if (!arg->subargs || !layout || layout_size <= 0)
    {
        *invalid_idx = -1;
        return APARSE_STATUS_FAILURE;
    }

    for (int idx = 0; idx < layout_size; idx++)
    {
        int offset = 0, size = 0;
        if (!aparse_arg_nend(arg_ptr))
        {
            *invalid_idx = -1;
            return 0;
        }
        if (!aparse_arg_is_argument(arg_ptr))
        {
            arg_ptr++;
            continue;
        }
        offset = layout[idx * 2];
        size   = layout[idx * 2 + 1];

        if (offset < 0 || size <= 0 ||
            offset < prev_offset + prev_size ||
            aparse_evaluate_size(arg_ptr) > size)
        {
            *invalid_idx = idx;
            return APARSE_STATUS_FAILURE;
        }

        prev_offset = offset;
        prev_size = size;
        arg_ptr++;
    }

    if (!aparse_arg_nend(arg_ptr))
    {
        *invalid_idx = -1;
        return APARSE_STATUS_FAILURE;
    }

    return APARSE_STATUS_OK;
}

static int aparse_fill_args_dest(
        const aparse_arg* arg, 
        uint8_t *buffer)
{
    int layout_size = 0;
    aparse_arg *arg_ptr = 0;
    if(!buffer)
        return APARSE_STATUS_OK;

    layout_size = arg->layout_size;
    if(layout_size == 0)
        return APARSE_STATUS_OK;

    arg_ptr = arg->subargs;
    for(int i = 0; i < layout_size && aparse_arg_nend(arg_ptr); 
            i++, arg_ptr++)
    {
        int offset = 0, size = 0;
        if(!aparse_arg_is_argument(arg_ptr))
            continue;
        offset  = arg->data_layout[i * 2];
        size    = arg->data_layout[i * 2 + 1];

        if(arg_ptr->ptr)
            continue;
        
        arg_ptr->ptr = &buffer[offset];
        if(arg_ptr->size == 0 && aparse_type_compare(arg_ptr, APARSE_ARG_TYPE_STRING))
            continue;
        arg_ptr->size = aparse_evaluate_size(arg_ptr);
    }
    return APARSE_STATUS_OK;
}

static void aparse_destroy_data(
        const aparse_arg* args, 
        char* composed)
{
    if(!args || !composed)
        return;
    aparse__foreach(sa, args)
    {
        if(!(sa->type & APARSE_ARG_TYPE_ARRAY))
            continue;
        free(((aparse_list*)sa->ptr)->ptr);
    }
}

// 0 no error, 1 error (just for cleaning up)
static int aparse_process_failure(
        const int status, 
        const aparse__context_t* context, 
        aparse_arg* args) 
{
    // Stop parsing and return
    if (status == APARSE_STATUS_FAILURE)
        return APARSE_STATUS_FAILURE;

    // Not using != here as if an array paramater was specified, the positional_count have already been -= 1
    if (context->positional_count > context->positional_processed) {
        // prepare required argument list
        aparse_list required_args = {.itemsz = sizeof(aparse_arg*)};
        for (; aparse_arg_nend(args); args++) {
            if (aparse_arg_is_positional(args) && !(args->flags & APARSE_ARG_PROCESSED)) {
                aparse_list_add(&required_args, &args);
            }
        }
        err_cb(APARSE_STATUS_MISSING_POSITIONAL, current_args, &required_args, err_userdata);
        aparse_list_free(&required_args);
        return APARSE_STATUS_FAILURE;
    }

    return APARSE_STATUS_OK;
}

static void aparse_default_error_callback(
        const aparse_status status, 
        const void* field1, 
        const void* field2, 
        void* userdata)
{
    switch(status)
    {
        case APARSE_STATUS_UNKNOWN_ARGUMENT:
        {
            const aparse_list* args = field1;
            fprintf(stderr, "%s: " __aparse_error_label ": unrecognized arguments: ", __aparse_progname);
            char** unknowns = args->ptr;
            for (size_t i = 0; i < args->size; i++, unknowns++) {
                if (i > 0) fprintf(stderr, ", ");
                fprintf(stderr, "%s", *unknowns);
            }
            fprintf(stderr, "\n");
            break;
        }
        case APARSE_STATUS_MISSING_VALUE:
        {
            const aparse_arg* arg = field1;
            const int expected_count = *(int*)field2;
            aparse_prog_error("option '%s' expected %d argument.",
                arg->flags & APARSE_ARG_SHORT_MATCH ? arg->shortopt : arg->longopt,
                expected_count);
            break;
        }
        case APARSE_STATUS_INVALID_VALUE:
        {
            const aparse_arg* arg = field1;
            const char* cargv = field2;
            aparse_prog_error("invalid %s '%s'",
                arg->type & APARSE_ARG_TYPE_FLOAT ? "float" : "integer", cargv);
            break;
        }
        case APARSE_STATUS_OVERFLOW:
        {
            const aparse_arg* arg = field1;
            const char* cargv = field2;
            aparse_library_error("value '%s' is too large for argument '%s'",
                                     cargv, arg->longopt ? arg->longopt : arg->shortopt);
            break;
        }
        case APARSE_STATUS_UNDERFLOW:
        {
            const aparse_arg* arg = field1;
            const char* cargv = field2;
            if(arg->type & APARSE_ARG_TYPE_FLOAT)
                aparse_library_warn("value '%s' underflows precision argument '%s'",
                                         cargv, arg->longopt ? arg->longopt : arg->shortopt);
            else
                aparse_library_error("value '%s' underflows argument '%s'",
                                         cargv, arg->longopt ? arg->longopt : arg->shortopt);
            break;
        }
        case APARSE_STATUS_MISSING_POSITIONAL:
        {
            const aparse_list* args = field2;
            aparse_print_usage();
            fprintf(stderr, "%s: " __aparse_error_label ": the following arguments are required: ", __aparse_progname);
            int printed = 0;
            aparse_arg* a = *(aparse_arg**)args->ptr;
            for (size_t i = 0; i < args->size; i++, a++) {
                
                if (aparse_arg_is_positional(a) && !(a->flags & APARSE_ARG_PROCESSED)) {
                    if (printed++ > 0) fprintf(stderr, ", ");
                    fprintf(stderr, "%s", a->longopt);
                }
            }
            fprintf(stderr, "\n");
            break;
        }
        case APARSE_STATUS_INVALID_SUBCOMMAND:
        {
            const aparse_list* args = field1;
            const char* cargv = field2;
            aparse_print_usage();
            fprintf(stderr, "%s: " __aparse_error_label ": invalid choice: '%s' (choose from ", __aparse_progname, cargv);
            aparse_arg* a = args->ptr;
            for(size_t i = 0; i < args->size; i++, a++)
                fprintf(stderr, "%s%s", a->longopt, i < (args->size - 1) ? ", " : "");
            fprintf(stderr, ")\n");
            break;
        }
        case APARSE_STATUS_NULL_POINTER:
        {
            const aparse_arg* arg = field1;
            aparse_library_warn("non-null pointers was expected of argument '%s', skipped.",
                arg->longopt ? arg->longopt : arg->shortopt);
            break;
        }
        case APARSE_STATUS_INVALID_TYPE:
        {
            const aparse_arg* arg = field1;
            aparse_library_error("invalid argument type: 0x%04X of argument '%s'",
                arg->type, arg->longopt ? arg->longopt : arg->shortopt);
            break;
        }
        case APARSE_STATUS_INVALID_SIZE:
        {
            const aparse_arg* arg = field1;
            const int size = *(int*)field2;
            aparse_library_error("invalid argument size: %d bytes of argument '%s'",
                size, arg->longopt ? arg->longopt : arg->shortopt);
            break;
        }
        case APARSE_STATUS_INVALID_LAYOUT:
        {
            const aparse_arg* arg = field1;
            const int index = *(int*)field2;
            aparse_library_error("layout of \"%s\" at index %d was invalid",
                    arg->longopt, index);
            break;
        }
        case APARSE_STATUS_ALLOC_FAILURE:
        {
            aparse_library_error("failed to allocate memory for parsing process, retry again.");
            break;
        }
        case APARSE_STATUS_UNHANDLED:
        {
            const aparse_arg* arg = field1;
            aparse_library_error("unhandled %s size: %d bytes of argument: '%s'",
                arg->type & APARSE_ARG_TYPE_FLOAT ? "float" : "integer",
                arg->size, arg->longopt ? arg->longopt : arg->shortopt
            );
            break;
        }
        default:
        {
            aparse_library_error("unhandled error message");
            break;
        }
    }
}

static void aparse__print_help(
        aparse_arg* main_args,
        aparse__context_t *ctx) 
{
    aparse_print_usage();
    printf("\n");
    if(root_args == current_args) 
    {
        if(prog_desc)
            printf("%s\n\n", prog_desc);
    }
    printf("positional arguments:\n");
    aparse_print_positional_help(main_args);
    printf("\noptions:\n");
    aparse_print_help_tag(&help_arg, INDENT);

    for(aparse_arg *sa = main_args;
            aparse_arg_nend(sa); sa++)
    {
        if (
                aparse_arg_is_argument(sa) && 
                !aparse_arg_is_positional(sa))
            aparse_print_help_tag(sa, INDENT);
    }
}
static void aparse__print_wrapped(
        const char *text,
        const int start_col,
        const int term_width)
{
    int usable_width = term_width - start_col;
    int col = 0;
    const char *p = text;

    printf("%*s", start_col, "");

    while (*p)
    {
        const char *word_start = NULL;
        int word_len = 0;

        if (*p == '\n')
        {
            putchar('\n');
            printf("%*s", start_col, "");
            col = 0;
            p++;
            continue;
        }

        while (*p && isspace((unsigned char)*p) && *p != '\n')
            p++;

        word_start = p;

        while (*p && !isspace((unsigned char)*p))
        {
            p++;
            word_len++;
        }

        if (word_len == 0)
            continue;

        int extra = (col > 0) ? 1 : 0;

        if (col && (col + extra + word_len > usable_width))
        {
            putchar('\n');
            printf("%*s", start_col, "");
            col = 0;
            extra = 0;
        }

        if (extra)
        {
            putchar(' ');
            col += 1;
        }

        fwrite(word_start, 1, word_len, stdout);
        col += word_len;
    }
}
static void aparse_print_help_tag(
        const aparse_arg* arg,
        const int indent)
{
    int len = 0;
    if (!aparse_arg_is_argument(arg) && !arg->help && !aparse_arg_nend(arg))
        return;


    len += printf("%*s", indent, "");
    if (arg->shortopt)
        len += printf("%s", arg->shortopt);
    if (arg->shortopt && arg->longopt)
        len += printf(", ");
    if (arg->longopt)
        len += printf("%s", arg->longopt);

    if (!aparse_arg_is_positional(arg) &&
        arg->type != APARSE_ARG_TYPE_BOOL)
    {
        const char* option = arg->longopt ? arg->longopt : arg->shortopt;
        size_t idx = aparse__option_value_index(option);

        putchar(' ');
        len += 1;

        for (const char* ch = option + idx; *ch != '\0'; ch++)
        {
            putchar(toupper((unsigned char)*ch));
            len += 1;
        }
    }

    bool longer = len > MAX_ARG_STR;

    if (arg->help)
    {
        int space = INDENT + (longer ? INDENT + MAX_ARG_STR : MAX_ARG_STR - len);

        if (longer)
            printf("\n");

        aparse__print_wrapped(arg->help, space, aparse_get_terminal_width());
    }

    printf("\n");
}

static void aparse_print_positional_help(aparse_arg* args) 
{
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

static char* aparse_construct_available_subcommands(
        const aparse_arg* args) 
{
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

static bool aparse_print_usage_before_r(
        aparse_arg* args,
        aparse_arg* target,
        aparse_list* strs)
{
    if (!args)
        return false;

    if (args == target)
        return true;

    for (aparse_arg* p = args; aparse_arg_nend(p); p++)
    {
        if (!aparse_arg_is_positional(p))
            continue;

        aparse_help_before entry = { p->longopt, false };
        aparse_list_add(strs, &entry);

        /* if this is the target, stop immediately */
        if (p == target)
            return true;

        /* recurse only into this branch */
        if (p->subargs)
        {
            if (aparse_print_usage_before_r(p->subargs, target, strs))
                return true;
        }

        strs->size--;
    }

    return false;
}

static void aparse_print_usage_before(
        aparse_arg* root, 
        aparse_arg* target) 
{
    aparse_list strs;
    aparse_list_new(&strs, 0, sizeof(aparse_help_before));

    aparse_print_usage_before_r(root, target, &strs);

    for (size_t i = 0; i < strs.size; i++) 
    {
        aparse_help_before* entry = &aparse_list_get(&strs, i, aparse_help_before);
        printf("%s ", entry->str);
        if (entry->freeable) 
            free((void*)entry->str);
    }

    aparse_list_free(&strs);
}

static void aparse_print_usage_after(aparse_arg* args)
{
    aparse_list list = {.itemsz = sizeof(aparse_arg*)};
    for(aparse_arg *sa = args;
            aparse_arg_nend(sa); sa++)
    {
        if(aparse_arg_is_positional(sa))
            aparse_list_add(&list, &sa);
        else {
            printf("[%s", sa->shortopt ? sa->shortopt : sa->longopt);
            if(!aparse_type_compare(sa, APARSE_ARG_TYPE_BOOL))
            {
                const char *option = 0;
                size_t idx = 0;
                
                option = sa->longopt ? sa->longopt : sa->shortopt;
                idx = aparse__option_value_index(option);
                putchar(' ');
                for(const char *ch = option + idx; *ch != '\0'; ch++)
                    putchar(toupper(*ch));
            }
            printf("] ");
        }
    }
    for(size_t i = 0; i < list.size; i++)
    {
        aparse_arg* entry = aparse_list_get(&list, i, aparse_arg*);
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

static void aparse_print_usage() 
{
    printf("usage: %s ", __aparse_progname);
    aparse_print_usage_before(root_args, current_args);
    aparse_print_usage_after(current_args);
}

static aparse_arg* aparse_argv_matching(
        const char* argv, 
        aparse_arg* args)
{
    if(
            !strcmp(argv, help_arg.shortopt) || 
            !strcmp(argv, help_arg.longopt)) {
        return &help_arg;
    }
    aparse_arg* positional = 0;
    for(aparse_arg *sa = args;
            aparse_arg_nend(sa); sa++)
    {
        if (aparse_arg_is_positional(sa)) {
            if (!(sa->flags & APARSE_ARG_PROCESSED))
                if(!positional)
                    positional = sa;
            continue;
        }
        if (sa->shortopt) {
            int shortlen = strlen(sa->shortopt);
            if (strncmp(argv, sa->shortopt, shortlen) == 0) {
                sa->flags |= APARSE_ARG_SHORT_MATCH;
                if (argv[shortlen] == '\0') {
                    return sa;
                } else if (argv[shortlen] == '=') {
                    sa->flags |= APARSE_ARG_EQUAL_VAL;
                    return sa;
                }
            }
        }
        if (sa->longopt) {
            int longlen = strlen(sa->longopt);
            if (strncmp(argv, sa->longopt, longlen) == 0) {
                sa->flags &= ~APARSE_ARG_SHORT_MATCH;
                if (argv[longlen] == '\0') {
                    return sa;
                } else if (argv[longlen] == '=') {
                    sa->flags |= APARSE_ARG_EQUAL_VAL;
                    return sa;
                }
            }
        }
    }
    return positional;
}

static const char* aparse_extract_exename(const char* argv0)
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

static size_t aparse__option_value_index(const char* longopt)
{
    const char* p = 0;
    size_t idx = 0;

    p = longopt;
    if(!p)
        return 0;

    while(p[idx] == '-' || p[idx] == '/')
        idx++;

    return idx;
}

static void aparse__context_init(
        aparse__context_t *ctx,
        const aparse__context_t* prev, 
        aparse_arg* args)
{
    ctx->idx_ptr = prev->idx_ptr;
    ctx->positional_count = aparse__count_positional(args);
    ctx->unknown = prev->unknown;
    ctx->dispatch = prev->dispatch;
    ctx->layer_idx = prev->layer_idx + 1;
    // aparse_library_info("%d", positional_count);
}

static int aparse__count_positional(
        aparse_arg *args)
{
    int count = 0;
    for(; aparse_arg_nend(args); args++) {
        if(aparse_arg_is_positional(args))
            count++;
        if(args->type & APARSE_ARG_TYPE_ARRAY && args->size == 0)
            count--;
    }
    return count;
}

static int aparse_get_terminal_width(void)
{
    int width = 0;
    char *env = NULL;

    env = getenv("COLUMNS");
    if (env)
    {
        width = atoi(env);
        if (width > 0)
            return width;
    }

#ifdef _WIN32
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
        HANDLE h = 0;

        h = GetStdHandle(STD_OUTPUT_HANDLE);

        if (h != INVALID_HANDLE_VALUE &&
            GetConsoleScreenBufferInfo(h, &csbi))
        {
            width = csbi.srWindow.Right - csbi.srWindow.Left + 1;

            if (width > 0)
                return width;
        }
    }

#else
    {
        struct winsize ws = {0};
        if (isatty(STDOUT_FILENO))
        {
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
            {
                if (ws.ws_col > 0)
                    return ws.ws_col;
            }
        }
    }
#endif

    return 80;
}
