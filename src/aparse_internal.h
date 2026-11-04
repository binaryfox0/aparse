#ifndef APARSE_INTERNAL_H
#define APARSE_INTERNAL_H

#include "aparse.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
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

#define APARSE__SPACE_PER_INDENT 2
#define MAX_ARG_STR 22
#define APARSE__MAX_DEPTH 16

#define APARSE_IS_LE (*(unsigned char *)&(uint16_t){1})
#define APARSE_ALLOC_SIZE 5

#define APARSE__MIN(a, b) ((a < b) ? (a) : (b))

#define aparse__lib__debug(...) \
    aparse_log("aparse", APARSE__DEBUG_LABEL, __VA_ARGS__)
#define aparse__lib__info(...) \
    aparse_log("aparse", APARSE__INFO_LABEL, __VA_ARGS__)
#define aparse__lib__warn(...) \
    aparse_log("aparse", APARSE__WARN_LABEL, __VA_ARGS__)
#define aparse__lib__error(...) \
    aparse_log("aparse", APARSE__ERROR_LABEL, __VA_ARGS__)

#define aparse__raise_fatal(ctx, type, field1, field2) \
    { \
        aparse__err_callback((ctx), (type), (field1), (field2), aparse__err_userdata); \
        return (type); \
    }

#define aparse__raise_nonfatal(ctx, type, field1, field2) \
        aparse__err_callback((ctx), (type), (field1), (field2), aparse__err_userdata);

#define aparse__foreach(child, parent) \
    for(aparse_arg *child = parent->subargs; aparse_arg_nend(child); child++)
#define aparse__tillend(element, start) \
    for(aparse_arg *element = start; aparse_arg_nend(element); element++)

typedef enum {
    APARSE__ARG_EQUAL_VAL   = (1 << 0),
    APARSE__ARG_SHORT_MATCH = (1 << 1),
    APARSE__ARG_PROCESSED   = (1 << 7)
} aparse_arg_state_t;

typedef struct 
{
    aparse_arg* args;
    void* payload;
    bool freeable;
} aparse__dispatch_t;

typedef struct aparse_context
{
    int idx;
    aparse_list *unknown;
    aparse_list *dispatch;
    int layer_idx;

    aparse_arg *stack[APARSE__MAX_DEPTH];
    int stack_top;
} aparse__context_t;

extern const char* aparse_progname;
extern const char *aparse__desc;
extern const aparse_arg aparse__help_arg;
extern aparse_error_callback aparse__err_callback;
extern void* aparse__err_userdata;

aparse_status aparse__parse_impl(
    const int argc,
    char* const * argv,
    aparse_arg* args,
    aparse__context_t* ctx);

APARSE__INLINE bool aparse__is_positional(
        const aparse_arg* arg) {
    return arg->type & APARSE_ARG_TYPE_POSITIONAL;
}

APARSE__INLINE bool aparse__is_argument(
        const aparse_arg* arg) {
    return arg->type & APARSE_ARG_TYPE_ARGUMENT;
}

static inline bool aparse__type_cmp(
        const aparse_arg* arg,
        const aparse_arg_types type) {
    return (arg->type & APARSE_ARG_TYPE_BITMASK) == type;
}

aparse_status aparse__check_missing(
        aparse_context* ctx,
        aparse_arg* args);

void aparse__default_errcb(
        const aparse_context* ctx,
        const aparse_status status, 
        const void* field1, 
        const void* field2, 
        void* userdata);

void aparse__print_help(
        aparse_arg* main_args,
        aparse__context_t *ctx);

void aparse__print_wrapped(
        const char *text, 
        int start_col, 
        int term_width);

void aparse__print_help_tag(
        const aparse_arg* arg, 
        const int indent
);

void aparse__print_pos_help(
        aparse_arg* args);

void aparse__print_subcmds(
        const aparse_arg* args);

void aparse__print_usage(
        const aparse_context *ctx);

void aparse__reset_state(
        aparse_arg *args);

aparse_arg* aparse__argv_match(
        const char* argv, 
        aparse_arg* args);

const char* aparse__get_exename(
        const char* argv0);
size_t aparse__option_value_index(const char* opt);

int aparse__get_term_width(void);

#endif
