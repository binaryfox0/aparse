#include <aparse/aparse.h>

#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <limits.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#    include <windows.h>
#    include <io.h>
#    include <sdkddkver.h>
#else
#    include <unistd.h>
#    include <sys/ioctl.h>
#endif

/* ========================================================================= */
/* MARK: Portability macros                                                  */
/* ========================================================================= */

/*
 * When ANSI escapes are disabled the macro must expand to an empty string
 * literal (not to nothing), otherwise call sites such as
 * `fputs(APARSE__ANSI_ESCAPE(...), f)` become syntax errors.
 */
#ifndef APARSE_NO_ANSI_ESCAPE
#    if defined(_WIN32) && !defined(__CYGWIN__)
#        if defined(NTDDI_VERSION) && (NTDDI_VERSION >= NTDDI_WIN10_TH2)
#            define APARSE__ANSI_ESCAPE(x) x
#        else
#            define APARSE__ANSI_ESCAPE(x) ""
#        endif
#    else
#        define APARSE__ANSI_ESCAPE(x) x
#    endif
#else
#    define APARSE__ANSI_ESCAPE(x) ""
#endif

#if defined(__GNUC__) || defined(__clang__)
#    define APARSE__PRINTF(fmt_index, arg_index) __attribute__((format(printf, fmt_index, arg_index)))
#    define APARSE__PRINTF_FMT
#    define APARSE__INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#    define APARSE__PRINTF(fmt_index, arg_index)
#    define APARSE__PRINTF_FMT _Printf_format_string_
#    define APARSE__INLINE __forceinline
#else
#    define APARSE__PRINTF(fmt_index, arg_index)
#    define APARSE__PRINTF_FMT
#    define APARSE__INLINE inline
#endif

#ifndef APARSE_PRINT_TO_STDOUT
#    define APARSE__STREAM stderr
#else
#    define APARSE__STREAM stdout
#endif

/* ========================================================================= */
/* MARK: Constants and helper macros                                         */
/* ========================================================================= */

#define APARSE__PROGRAM_NAME_MAX 256
#define APARSE__DEFAULT_HELP_POSITION 24
#define APARSE__SPACE_PER_INDENT 2

#define APARSE__CONST_CAST(type, x) ((type)(uintptr_t)(x))
#define APARSE__MAX(a, b) ((a) > (b) ? (a) : (b))
#define APARSE__MIN(a, b) ((a) < (b) ? (a) : (b))
#define APARSE__NULL_TO_EMPTY(x) ((x) ? (x) : "")
#define APARSE__NARGS_IS_FLEXIBLE(nargs) \
    ((nargs) == APARSE_NARGS_MINIMUM || (nargs) == APARSE_NARGS_MAXIMUM)

#define APARSE__COLOR_DEFAULT       "\x1b[0m"
#define APARSE__COLOR_GREEN         "\x1b[32m"
#define APARSE__COLOR_YELLOW        "\x1b[33m"
#define APARSE__COLOR_CYAN          "\x1b[36m"
#define APARSE__COLOR_BOLD_RED      "\x1b[1;31m"
#define APARSE__COLOR_BOLD_GREEN    "\x1b[1;32m"
#define APARSE__COLOR_BOLD_YELLOW   "\x1b[1;33m"
#define APARSE__COLOR_BOLD_BLUE     "\x1b[1;34m"
#define APARSE__COLOR_BOLD_MAGENTA  "\x1b[1;35m"
#define APARSE__COLOR_BOLD_CYAN     "\x1b[1;36m"
#define APARSE__COLORIZE(color, str) APARSE__ANSI_ESCAPE(color) str APARSE__ANSI_ESCAPE(APARSE__COLOR_DEFAULT)

/* ========================================================================= */
/* MARK: Internal types                                                      */
/* ========================================================================= */

typedef struct
{
    size_t processed_count;
    size_t item_idx;
} aparse__argument_state_t;

typedef struct
{
    const aparse_argument_t *args;
    size_t args_count;
    aparse__argument_state_t *states;
} aparse__scope_t;

typedef struct
{
    size_t argc;
    char *const *argv;
    size_t current_idx;
    size_t depth;
    aparse_stack_frame_t stack[APARSE_MAX_DEPTH];
} aparse__persistent_state_t;

typedef enum
{
    APARSE__SPECIAL_ACTION_HELP = 1,
    APARSE__SPECIAL_ACTION_VERSION
} aparse__special_action_t;

#define APARSE__ARG(x) ((const aparse_argument_t *)(uintptr_t)(x))

/* ========================================================================= */
/* MARK: Logging                                                             */
/* ========================================================================= */

#define APARSE__DEBUG_LABEL APARSE__COLORIZE(APARSE__COLOR_BOLD_CYAN, "debug")
#define APARSE__INFO_LABEL  APARSE__COLORIZE(APARSE__COLOR_BOLD_BLUE, "info")
#define APARSE__WARN_LABEL  APARSE__COLORIZE(APARSE__COLOR_BOLD_YELLOW, "warn")
#define APARSE__ERROR_LABEL APARSE__COLORIZE(APARSE__COLOR_BOLD_RED, "error")
#define APARSE__DEBUG(source, ...) aparse_log(source, APARSE__DEBUG_LABEL, __VA_ARGS__)
#define APARSE__INFO(source, ...)  aparse_log(source, APARSE__INFO_LABEL, __VA_ARGS__)
#define APARSE__WARN(source, ...)  aparse_log(source, APARSE__WARN_LABEL, __VA_ARGS__)
#define APARSE__ERROR(source, ...) aparse_log(source, APARSE__ERROR_LABEL, __VA_ARGS__)

/* Print the "<source>: <type>: " prefix shared by all diagnostics. */
static void aparse__log_prefix(const char *source, const char *type)
{
    fputs(source ? source : "aparse", APARSE__STREAM);
    fputs(": ", APARSE__STREAM);
    fputs(type ? type : "", APARSE__STREAM);
    fputs(": ", APARSE__STREAM);
}

static APARSE__PRINTF(3, 4) void aparse_log(
        const char *source,
        const char *type,
        APARSE__PRINTF_FMT const char *fmt,
        ...)
{
    va_list va;

    if(!fmt)
        return;

    aparse__log_prefix(source, type);
    va_start(va, fmt);
    vfprintf(APARSE__STREAM, fmt, va);
    va_end(va);
    fputc('\n', APARSE__STREAM);
}

/* ========================================================================= */
/* MARK: Allocation and configuration                                       */
/* ========================================================================= */

static void *aparse__default_malloc(void *userdata, size_t size)
{
    const aparse_alloc_t *alloc = (const aparse_alloc_t *)userdata;

    if(!alloc)
        return malloc(size);

    return alloc->malloc ? alloc->malloc(alloc->userdata, size) :
        (alloc->realloc ? alloc->realloc(alloc->userdata, NULL, size) : malloc(size));
}

static void *aparse__default_realloc(void *userdata, void *ptr, size_t size)
{
    const aparse_alloc_t *alloc = (const aparse_alloc_t *)userdata;

    if(!alloc)
        return realloc(ptr, size);

    return alloc->realloc ? alloc->realloc(alloc->userdata, ptr, size) : realloc(ptr, size);
}

static void aparse__default_free(void *userdata, void *ptr)
{
    const aparse_alloc_t *alloc = (const aparse_alloc_t *)userdata;

    if(!alloc)
    {
        free(ptr);
        return;
    }

    if(alloc->free)
        alloc->free(alloc->userdata, ptr);
    else
        free(ptr);
}

const aparse_config_t aparse_default_config =
{
    .alloc.malloc = aparse__default_malloc,
    .alloc.realloc = aparse__default_realloc,
    .alloc.free = aparse__default_free,
    .help.shortopt = "-h",
    .help.longopt = "--help",
    .version.shortopt = "-v",
    .version.longopt = "--version"
};

static APARSE__INLINE bool aparse__config_is_valid(const aparse_config_t *config)
{
    return config && config->alloc.malloc && config->alloc.realloc && config->alloc.free;
}

/* ========================================================================= */
/* MARK: Error strings                                                       */
/* ========================================================================= */

const char *aparse_strerror(const aparse_error_t err)
{
    static const char *const strings[APARSE__ERR_COUNT] =
    {
        [APARSE_ERR_OK]                  = "success",
        [APARSE_ERR_UNIMPLEMENTED]       = "operation is not implemented",
        [APARSE_ERR_INVALID_ARGUMENT]    = "invalid argument",
        [APARSE_ERR_INVALID_SPEC]        = "invalid argument specification",
        [APARSE_ERR_DUPLICATE_NAME]      = "duplicate argument name",
        [APARSE_ERR_INVALID_TYPE]        = "invalid argument type",
        [APARSE_ERR_INVALID_SIZE]        = "invalid argument size",
        [APARSE_ERR_INVALID_DESTINATION] = "invalid argument destination",
        [APARSE_ERR_OUT_OF_MEMORY]       = "out of memory",
        [APARSE_ERR_SIZE_OVERFLOW]       = "size calculation overflow",
        [APARSE_ERR_INVALID_CMDLINE]     = "invalid command-line string",
        [APARSE_ERR_INVALID_ENCODING]    = "invalid character encoding",
        [APARSE_ERR_INTERNAL]            = "internal error",
        [APARSE_ERR_USER]                = "user error",
        [APARSE_ERR_TOO_DEEP]            = "subcommand nesting is too deep"
    };

    if((int)err < 0 || err >= APARSE__ERR_COUNT)
        return "unknown error";

    return strings[err] ? strings[err] : "unassigned error";
}

const char *aparse_user_strerror(const aparse_user_error_t err)
{
    static const char *const strings[APARSE__UERR_COUNT] =
    {
        [APARSE_UERR_OK]                  = "success",
        [APARSE_UERR_HELP]                = "user requested help output",
        [APARSE_UERR_VERSION]             = "user requested version output",
        [APARSE_UERR_MISSING_VALUE]       = "option value was not provided",
        [APARSE_UERR_INVALID_VALUE]       = "option value is invalid",
        [APARSE_UERR_VALUE_OVERFLOW]      = "option value exceeds the maximum representable value",
        [APARSE_UERR_VALUE_UNDERFLOW]     = "option value is below the minimum representable value",
        [APARSE_UERR_VERIFICATION]        = "option value failed verification",
        [APARSE_UERR_UNEXPECTED_ARGUMENT] = "unexpected command-line argument",
        [APARSE_UERR_MISSING_POSITIONAL]  = "required positional argument was not provided",
        [APARSE_UERR_UNKNOWN_COMMAND]     = "unrecognized subcommand",
    };

    if((int)err < 0 || err >= APARSE__UERR_COUNT)
        return "unknown user error";

    return strings[err] ? strings[err] : "unassigned user error";
}

/* ========================================================================= */
/* MARK: Argument helpers                                                    */
/* ========================================================================= */

/* Name of a positional/command as shown to the user: metavar wins. */
static APARSE__INLINE const char *aparse__positional_name(const aparse_argument_t *arg)
{
    if(!arg)
        return "";
    if(arg->metavar)
        return arg->metavar;
    if(arg->longopt)
        return arg->longopt;
    if(arg->shortopt)
        return arg->shortopt;
    return "";
}

/* Name of an option/flag as shown to the user in diagnostics. */
static APARSE__INLINE const char *aparse__arg_display_name(const aparse_argument_t *arg)
{
    if(!arg)
        return "";
    if(arg->longopt)
        return arg->longopt;
    if(arg->shortopt)
        return arg->shortopt;
    if(arg->metavar)
        return arg->metavar;
    return "";
}

static APARSE__INLINE bool aparse__arg_is_array(const aparse_argument_t *arg)
{
    return arg && arg->kind == APARSE_ARGUMENT_OPTION &&
           (arg->action == APARSE_ACTION_APPEND || APARSE__NARGS_IS_FLEXIBLE(APARSE_NARGS_MODE(arg->nargs)));
}

static APARSE__INLINE bool aparse__arg_is_special(const aparse_argument_t *arg)
{
    return arg == APARSE__ARG(APARSE__SPECIAL_ACTION_HELP) ||
           arg == APARSE__ARG(APARSE__SPECIAL_ACTION_VERSION);
}

static APARSE__INLINE bool aparse__arg_is_positional(const aparse_argument_t *arg)
{
    return arg && !aparse__arg_is_special(arg) &&
           (arg->kind == APARSE_ARGUMENT_SUBPARSERS || arg->kind == APARSE_ARGUMENT_POSITIONAL);
}

/* Flags and options, as opposed to positionals and subcommands. */
static APARSE__INLINE bool aparse__arg_is_optional(const aparse_argument_t *arg)
{
    return arg && (arg->kind == APARSE_ARGUMENT_FLAG || arg->kind == APARSE_ARGUMENT_OPTION);
}

/* Find the subcommand of a SUBPARSERS argument called `name`. */
static const aparse_argument_t *aparse__find_subcommand(
        const aparse_argument_t *subparsers,
        const char *name)
{
    if(!subparsers || !subparsers->children || !name)
        return NULL;

    for(size_t i = 0; i < subparsers->children_count; i++)
    {
        const aparse_argument_t *child = &subparsers->children[i];
        if(child->longopt && strcmp(name, child->longopt) == 0)
            return child;
    }

    return NULL;
}

/* Find the subcommand of a SUBPARSERS argument whose scope is `scope_args`. */
static const aparse_argument_t *aparse__find_subcommand_by_scope(
        const aparse_argument_t *subparsers,
        const aparse_argument_t *scope_args)
{
    if(!subparsers || subparsers->kind != APARSE_ARGUMENT_SUBPARSERS || !subparsers->children)
        return NULL;

    for(size_t i = 0; i < subparsers->children_count; i++)
        if(subparsers->children[i].children == scope_args)
            return &subparsers->children[i];

    return NULL;
}

/* ========================================================================= */
/* MARK: Scope management                                                    */
/* ========================================================================= */

static aparse_error_t aparse__scope_create(
        const aparse_argument_t *args,
        size_t args_count,
        const aparse_config_t *config,
        aparse__scope_t *out_scope)
{
    if(!out_scope || (!args && args_count > 0) || !aparse__config_is_valid(config))
        return APARSE_ERR_INVALID_ARGUMENT;

    memset(out_scope, 0, sizeof(*out_scope));
    out_scope->args = args;
    out_scope->args_count = args_count;

    /* An empty scope is valid (e.g. a subcommand without arguments). */
    if(args_count == 0)
        return APARSE_ERR_OK;
    if(args_count > (size_t)-1 / sizeof(*out_scope->states))
    {
        memset(out_scope, 0, sizeof(*out_scope));
        return APARSE_ERR_SIZE_OVERFLOW;
    }

    out_scope->states = config->alloc.malloc(
            config->alloc.userdata, sizeof(*out_scope->states) * args_count);
    if(!out_scope->states)
    {
        memset(out_scope, 0, sizeof(*out_scope));
        return APARSE_ERR_OUT_OF_MEMORY;
    }

    memset(out_scope->states, 0, sizeof(*out_scope->states) * args_count);
    return APARSE_ERR_OK;
}

static APARSE__INLINE aparse__argument_state_t *aparse__scope_get_state(
        aparse__scope_t *scope,
        const aparse_argument_t *arg)
{
    if(!scope || !scope->states || !arg || arg < scope->args || arg >= scope->args + scope->args_count)
        return NULL;

    return &scope->states[arg - scope->args];
}

static void aparse__scope_destroy(aparse__scope_t *scope, const aparse_config_t *config)
{
    if(!scope)
        return;
    if(scope->states && config && config->alloc.free)
        config->alloc.free(config->alloc.userdata, scope->states);
    memset(scope, 0, sizeof(*scope));
}

/* ========================================================================= */
/* MARK: Argument matching                                                   */
/* ========================================================================= */

/*
 * Match `name` against `carg`. With `allow_equal`, `carg` may also be of the
 * form `name=value` (used by options only).
 */
static APARSE__INLINE bool aparse__name_matches(
        const char *name,
        const char *carg,
        const bool allow_equal)
{
    size_t len = 0;

    if(!name || !carg)
        return false;
    len = strlen(name);
    if(len == 0 || strncmp(name, carg, len) != 0)
        return false;

    return carg[len] == '\0' || (allow_equal && carg[len] == '=');
}

static APARSE__INLINE bool aparse__match_arg_single(
        const char *shortopt,
        const char *longopt,
        const char *carg,
        const bool allow_equal)
{
    return aparse__name_matches(shortopt, carg, allow_equal) ||
           aparse__name_matches(longopt, carg, allow_equal);
}

static APARSE__INLINE const aparse_argument_t *aparse__match_arg(
        aparse__scope_t *scope,
        const char *carg,
        const aparse_config_t *config)
{
    const aparse_argument_t *matched = NULL;

    if(!scope || !carg || !config)
        return NULL;
    if(aparse__match_arg_single(config->help.shortopt, config->help.longopt, carg, false))
        return APARSE__ARG(APARSE__SPECIAL_ACTION_HELP);
    if(aparse__match_arg_single(config->version.shortopt, config->version.longopt, carg, false))
        return APARSE__ARG(APARSE__SPECIAL_ACTION_VERSION);

    for(size_t i = 0; i < scope->args_count; i++)
    {
        const aparse_argument_t *arg = &scope->args[i];

        if(aparse__arg_is_positional(arg))
        {
            /* Only the first not-yet-processed positional can take a value. */
            if(!matched && scope->states[i].processed_count == 0)
                matched = arg;
            continue;
        }

        if(aparse__match_arg_single(arg->shortopt, arg->longopt, carg,
                    arg->kind == APARSE_ARGUMENT_OPTION))
            return arg;
    }

    return matched;
}

/* ========================================================================= */
/* MARK: Value processing                                                    */
/* ========================================================================= */

static aparse_error_t aparse__value_error(
        aparse_result_t *result,
        const aparse_argument_t *arg,
        const char *value,
        const aparse_user_error_t error)
{
    result->user_error = error;
    result->arg = arg;
    result->value = value;
    return APARSE_ERR_USER;
}

#define APARSE__STORE_INTEGER(utype, stype, umax, smin, smax) \
    do { \
        if(is_unsigned) \
        { \
            if(num > (uint64_t)(umax)) \
                return aparse__value_error(result, arg, carg, APARSE_UERR_VALUE_OVERFLOW); \
            *(utype *)arg->out_val = (utype)num; \
        } \
        else \
        { \
            const uint64_t sign_bit = (uint64_t)(smax) + UINT64_C(1); \
            if(is_negative) \
            { \
                if(num > sign_bit) \
                    return aparse__value_error(result, arg, carg, APARSE_UERR_VALUE_UNDERFLOW); \
                *(stype *)arg->out_val = num == sign_bit ? (stype)(smin) : (stype)-(stype)num; \
            } \
            else \
            { \
                if(num > (uint64_t)(smax)) \
                    return aparse__value_error(result, arg, carg, APARSE_UERR_VALUE_OVERFLOW); \
                *(stype *)arg->out_val = (stype)num; \
            } \
        } \
        return APARSE_ERR_OK; \
    } while(0)

/*
 * Increment through the unsigned type so that wrap-around is well defined
 * (signed overflow would be undefined behaviour).
 */
#define APARSE__INCREMENT_INTEGER(utype) \
    do { ++*(utype *)arg->out_val; return APARSE_ERR_OK; } while(0)

static aparse_error_t aparse__increment_integer(const aparse_argument_t *arg)
{
    if(!arg)
        return APARSE_ERR_INVALID_ARGUMENT;
    if(!arg->out_val)
        return APARSE_ERR_INVALID_DESTINATION;
    if(arg->type != APARSE_TYPE_INT && arg->type != APARSE_TYPE_UINT)
        return APARSE_ERR_INVALID_TYPE;

    switch(arg->size)
    {
        case sizeof(uint8_t):  APARSE__INCREMENT_INTEGER(uint8_t);
        case sizeof(uint16_t): APARSE__INCREMENT_INTEGER(uint16_t);
        case sizeof(uint32_t): APARSE__INCREMENT_INTEGER(uint32_t);
        case sizeof(uint64_t): APARSE__INCREMENT_INTEGER(uint64_t);
        default: return APARSE_ERR_INVALID_SIZE;
    }
}

static aparse_error_t aparse__process_integer(
        const aparse_argument_t *arg,
        const char *carg,
        aparse_result_t *result)
{
    const char *p = carg;
    char *endptr = NULL;
    bool is_unsigned = false;
    bool is_negative = false;
    int base = 10;
    unsigned long long parsed = 0;
    uint64_t num = 0;

    if(!arg || !carg || !result)
        return APARSE_ERR_INVALID_ARGUMENT;
    if(!arg->out_val)
        return APARSE_ERR_INVALID_DESTINATION;
    if(arg->type != APARSE_TYPE_INT && arg->type != APARSE_TYPE_UINT)
        return APARSE_ERR_INVALID_TYPE;
    if(arg->size != sizeof(uint8_t) && arg->size != sizeof(uint16_t) &&
       arg->size != sizeof(uint32_t) && arg->size != sizeof(uint64_t))
        return APARSE_ERR_INVALID_SIZE;

    is_unsigned = arg->type == APARSE_TYPE_UINT;
    if(*p == '-' || *p == '+')
    {
        is_negative = *p == '-';
        p++;
    }
    if(is_negative && is_unsigned)
        return aparse__value_error(result, arg, carg, APARSE_UERR_INVALID_VALUE);
    if(*p == '\0' || *p == '+' || *p == '-' || isspace((unsigned char)*p))
        return aparse__value_error(result, arg, carg, APARSE_UERR_INVALID_VALUE);

    if(p[0] == '0' && p[1] != '\0')
    {
        switch(tolower((unsigned char)p[1]))
        {
            case 'x': base = 16; p += 2; break;
            case 'b': base = 2;  p += 2; break;
            case 'o': base = 8;  p += 2; break;
            default:  base = 8;          break;
        }
    }

    /*
     * Re-validate after the base prefix: strtoull() itself accepts leading
     * whitespace and signs, so "0x-5" or "0x" must be rejected here.
     */
    if(!isalnum((unsigned char)*p))
        return aparse__value_error(result, arg, carg, APARSE_UERR_INVALID_VALUE);

    errno = 0;
    parsed = strtoull(p, &endptr, base);
    if(endptr == p || *endptr != '\0')
        return aparse__value_error(result, arg, carg, APARSE_UERR_INVALID_VALUE);
    if(errno == ERANGE || parsed > (unsigned long long)UINT64_MAX)
        return aparse__value_error(result, arg, carg,
                is_negative ? APARSE_UERR_VALUE_UNDERFLOW : APARSE_UERR_VALUE_OVERFLOW);

    num = (uint64_t)parsed;
    switch(arg->size)
    {
        case sizeof(uint8_t):
            APARSE__STORE_INTEGER(uint8_t, int8_t, UINT8_MAX, INT8_MIN, INT8_MAX);
        case sizeof(uint16_t):
            APARSE__STORE_INTEGER(uint16_t, int16_t, UINT16_MAX, INT16_MIN, INT16_MAX);
        case sizeof(uint32_t):
            APARSE__STORE_INTEGER(uint32_t, int32_t, UINT32_MAX, INT32_MIN, INT32_MAX);
        case sizeof(uint64_t):
            APARSE__STORE_INTEGER(uint64_t, int64_t, UINT64_MAX, INT64_MIN, INT64_MAX);
        default:
            return APARSE_ERR_INVALID_SIZE;
    }
}

#undef APARSE__STORE_INTEGER
#undef APARSE__INCREMENT_INTEGER

static aparse_error_t aparse__process_float(
        const aparse_argument_t *arg,
        const char *carg,
        aparse_result_t *result)
{
    char *endptr = NULL;
    long double value = 0.0L;
    long double max_value = 0.0L;

    if(!arg || !carg || !result)
        return APARSE_ERR_INVALID_ARGUMENT;
    if(!arg->out_val)
        return APARSE_ERR_INVALID_DESTINATION;
    if(arg->type != APARSE_TYPE_FLOAT && arg->type != APARSE_TYPE_DOUBLE)
        return APARSE_ERR_INVALID_TYPE;
    if((arg->type == APARSE_TYPE_FLOAT && arg->size != sizeof(float)) ||
       (arg->type == APARSE_TYPE_DOUBLE && arg->size != sizeof(double)))
        return APARSE_ERR_INVALID_SIZE;
    if(*carg == '\0' || isspace((unsigned char)*carg))
        return aparse__value_error(result, arg, carg, APARSE_UERR_INVALID_VALUE);

    errno = 0;
    value = strtold(carg, &endptr);
    if(endptr == carg || *endptr != '\0')
        return aparse__value_error(result, arg, carg, APARSE_UERR_INVALID_VALUE);
    if(errno == ERANGE)
    {
        if(isinf(value))
            return aparse__value_error(result, arg, carg, APARSE_UERR_VALUE_OVERFLOW);
        if(value == 0.0L)
            return aparse__value_error(result, arg, carg, APARSE_UERR_VALUE_UNDERFLOW);
    }

    max_value = arg->type == APARSE_TYPE_FLOAT ? (long double)FLT_MAX : (long double)DBL_MAX;
    if(isfinite(value) && fabsl(value) > max_value)
        return aparse__value_error(result, arg, carg, APARSE_UERR_VALUE_OVERFLOW);

    if(arg->type == APARSE_TYPE_FLOAT)
    {
        float converted = (float)value;
        if(value != 0.0L && converted == 0.0f)
            return aparse__value_error(result, arg, carg, APARSE_UERR_VALUE_UNDERFLOW);
        *(float *)arg->out_val = converted;
    }
    else
    {
        double converted = (double)value;
        if(value != 0.0L && converted == 0.0)
            return aparse__value_error(result, arg, carg, APARSE_UERR_VALUE_UNDERFLOW);
        *(double *)arg->out_val = converted;
    }

    return APARSE_ERR_OK;
}

static aparse_error_t aparse__process_positional(
        const aparse_argument_t *arg,
        const char *carg,
        aparse_result_t *result)
{
    aparse_error_t rc = APARSE_ERR_OK;

    if(!arg || !carg || !result)
        return APARSE_ERR_INVALID_ARGUMENT;
    if(!arg->out_val)
        return APARSE_ERR_INVALID_DESTINATION;

    switch(arg->type)
    {
        case APARSE_TYPE_STRING:
        {
            size_t len = 0;

            if(arg->size == APARSE_STRING_UNBOUNDED)
            {
                *(const char **)arg->out_val = carg;
                return APARSE_ERR_OK;
            }

            /* The buffer must hold the string plus its terminating NUL. */
            len = strlen(carg);
            if(len >= arg->size)
            {
                rc = aparse__value_error(result, arg, carg, APARSE_UERR_VALUE_OVERFLOW);
                break;
            }
            memcpy(arg->out_val, carg, len + 1);
            break;
        }

        case APARSE_TYPE_INT:
        case APARSE_TYPE_UINT:
            rc = aparse__process_integer(arg, carg, result);
            break;

        case APARSE_TYPE_FLOAT:
        case APARSE_TYPE_DOUBLE:
            rc = aparse__process_float(arg, carg, result);
            break;

        default:
            return APARSE_ERR_INVALID_TYPE;
    }

    if(rc == APARSE_ERR_OK && arg->verify.verify_fn)
    {
        result->verification_error = arg->verify.verify_fn(arg->verify.userdata, arg, arg->out_val);
        if(result->verification_error)
            return aparse__value_error(result, arg, carg, APARSE_UERR_VERIFICATION);
    }

    return rc;
}

/* ========================================================================= */
/* MARK: Array storage                                                       */
/* ========================================================================= */

static APARSE__INLINE size_t aparse__estimate_elem_size(const aparse_argument_t *arg)
{
    return arg->type == APARSE_TYPE_STRING && arg->size == APARSE_STRING_UNBOUNDED ?
        sizeof(const char *) : arg->size;
}

static aparse_error_t aparse__array_reserve(
        aparse_array_t *array,
        const size_t elem_size,
        const size_t size,
        const aparse_config_t *config)
{
    size_t new_capacity = 0;
    size_t max_capacity = 0;
    void *new_data = NULL;

    if(!array || !elem_size || !config || !config->alloc.realloc)
        return APARSE_ERR_INVALID_ARGUMENT;
    if(size <= array->capacity)
        return APARSE_ERR_OK;

    max_capacity = (size_t)-1 / elem_size;
    if(size > max_capacity)
        return APARSE_ERR_SIZE_OVERFLOW;

    new_capacity = array->capacity ? array->capacity : APARSE__MIN(4, max_capacity);
    while(new_capacity < size)
    {
        if(new_capacity > max_capacity / 2)
        {
            new_capacity = max_capacity;
            break;
        }
        new_capacity *= 2;
    }

    new_data = config->alloc.realloc(config->alloc.userdata, array->data, new_capacity * elem_size);
    if(!new_data)
        return APARSE_ERR_OUT_OF_MEMORY;

    array->data = new_data;
    array->capacity = new_capacity;
    return APARSE_ERR_OK;
}

/* ========================================================================= */
/* MARK: Option processing                                                   */
/* ========================================================================= */

static aparse_error_t aparse__process_option(
        const aparse_argument_t *arg,
        const char *carg,
        const aparse_config_t *config,
        aparse__scope_t *scope,
        aparse__persistent_state_t *persistent,
        aparse_result_t *result)
{
    aparse__argument_state_t *state = NULL;
    const char *equal = NULL;
    size_t nargs_mode = 0;
    size_t nargs_count = 0;
    size_t available = 0;
    size_t count = 0;
    size_t elem_size = 0;
    size_t required_capacity = 0;
    void *array_ptr = NULL;
    aparse_array_t *out_arr = NULL;
    aparse_argument_t arg_copy = {0};
    aparse_error_t err = APARSE_ERR_OK;

    if(!arg || !carg || !aparse__config_is_valid(config) || !scope || !persistent || !result)
        return APARSE_ERR_INVALID_ARGUMENT;
    if(!arg->out_val)
        return APARSE_ERR_INVALID_DESTINATION;
    if(arg->action != APARSE_ACTION_STORE && arg->action != APARSE_ACTION_APPEND)
        return APARSE_ERR_INVALID_SPEC;

    nargs_mode = APARSE_NARGS_MODE(arg->nargs);
    nargs_count = APARSE_NARGS_COUNT(arg->nargs);
    equal = strchr(carg, '=');
    available = equal != NULL;

    /* Count the values that follow, up to the next option. */
    for(size_t i = persistent->current_idx; i < persistent->argc; i++)
    {
        const aparse_argument_t *matched_arg = NULL;

        if(!persistent->argv[i])
            break;
        matched_arg = aparse__match_arg(scope, persistent->argv[i], config);
        if(matched_arg && !aparse__arg_is_positional(matched_arg))
            break;
        available++;
    }

    switch(nargs_mode)
    {
        case APARSE_NARGS_NONE:
            result->expected_count = 1;
            if(available == 0)
                return aparse__value_error(result, arg, NULL, APARSE_UERR_MISSING_VALUE);
            count = 1;
            break;

        case APARSE_NARGS_EXACT:
            if(!nargs_count)
                return APARSE_ERR_INVALID_SPEC;
            result->expected_count = nargs_count;
            if(available < nargs_count)
                return aparse__value_error(result, arg, NULL, APARSE_UERR_MISSING_VALUE);
            count = nargs_count;
            break;

        case APARSE_NARGS_MINIMUM:
            result->expected_count = nargs_count;
            if(available < nargs_count)
                return aparse__value_error(result, arg, NULL, APARSE_UERR_MISSING_VALUE);
            count = available;
            break;

        case APARSE_NARGS_MAXIMUM:
            if(!nargs_count)
                return APARSE_ERR_INVALID_SPEC;
            count = available < nargs_count ? available : nargs_count;
            break;

        default:
            return APARSE_ERR_INVALID_SPEC;
    }

    elem_size = aparse__estimate_elem_size(arg);
    state = aparse__scope_get_state(scope, arg);
    if(!state)
        return APARSE_ERR_INVALID_ARGUMENT;
    if(arg->action == APARSE_ACTION_STORE)
        state->item_idx = 0;

    /* Flexible options may legitimately receive zero values. */
    if(count == 0)
        return APARSE_ERR_OK;

    if(aparse__arg_is_array(arg))
    {
        out_arr = arg->out_val;
        if(state->item_idx > (size_t)-1 - count)
            return APARSE_ERR_SIZE_OVERFLOW;
        required_capacity = state->item_idx + count;
        err = aparse__array_reserve(out_arr, elem_size, required_capacity, config);
        if(err != APARSE_ERR_OK)
            return err;
        array_ptr = out_arr->data;
    }
    else
        array_ptr = arg->out_val;

    if(!array_ptr)
        return APARSE_ERR_INVALID_DESTINATION;

    arg_copy = *arg;
    if(equal)
    {
        arg_copy.out_val = (uint8_t *)array_ptr + elem_size * state->item_idx++;
        err = aparse__process_positional(&arg_copy, equal + 1, result);
        if(err != APARSE_ERR_OK)
        {
            result->arg = result->arg == &arg_copy ? arg : result->arg;
            return err;
        }
        count--;
    }

    while(count--)
    {
        if(persistent->current_idx >= persistent->argc || !persistent->argv[persistent->current_idx])
            return APARSE_ERR_INTERNAL;

        arg_copy.out_val = (uint8_t *)array_ptr + elem_size * state->item_idx++;
        err = aparse__process_positional(&arg_copy, persistent->argv[persistent->current_idx++], result);
        if(err != APARSE_ERR_OK)
        {
            result->arg = result->arg == &arg_copy ? arg : result->arg;
            return err;
        }
    }

    return APARSE_ERR_OK;
}

/* ========================================================================= */
/* MARK: Result bookkeeping                                                  */
/* ========================================================================= */

static aparse_error_t aparse__add_string_to_result(
        aparse_result_t *result,
        const char *str,
        const aparse_config_t *config)
{
    if(!result || !str || !aparse__config_is_valid(config))
        return APARSE_ERR_INVALID_ARGUMENT;

    if(result->strings_count >= result->strings_capacity)
    {
        size_t new_capacity = result->strings_capacity ? result->strings_capacity * 2 : 4;
        const char **new_strings = NULL;

        if(new_capacity > (size_t)-1 / sizeof(*result->strings))
            return APARSE_ERR_SIZE_OVERFLOW;

        new_strings = config->alloc.realloc(config->alloc.userdata, result->strings,
                sizeof(*result->strings) * new_capacity);
        if(!new_strings)
            return APARSE_ERR_OUT_OF_MEMORY;
        result->strings = new_strings;
        result->strings_capacity = new_capacity;
    }

    result->strings[result->strings_count++] = str;
    return APARSE_ERR_OK;
}

/* Append a subcommand node, keeping the list outermost-to-innermost. */
static aparse_error_t aparse__append_subcommand(
        aparse_result_t *result,
        const aparse_argument_t *subcmd,
        const aparse_config_t *config)
{
    aparse_subcommand_node_t *node = NULL;
    aparse_subcommand_node_t **tail = &result->subcommand_head;

    node = config->alloc.malloc(config->alloc.userdata, sizeof(*node));
    if(!node)
        return APARSE_ERR_OUT_OF_MEMORY;

    memset(node, 0, sizeof(*node));
    node->command = subcmd->children;
    while(*tail)
        tail = &(*tail)->next;
    *tail = node;

    return APARSE_ERR_OK;
}

/* ========================================================================= */
/* MARK: Parser core                                                         */
/* ========================================================================= */

static void aparse__apply_default(const aparse_argument_t *arg)
{
    if(!arg || !arg->out_val || !arg->default_val)
        return;

    /* Subcommands have no value, and array destinations hold an aparse_array_t. */
    if(arg->kind == APARSE_ARGUMENT_SUBPARSERS || aparse__arg_is_array(arg))
        return;

    switch(arg->type)
    {
        case APARSE_TYPE_STRING:
            if(arg->size == APARSE_STRING_UNBOUNDED)
                *(const char **)arg->out_val = *(const char *const *)arg->default_val;
            else
            {
                /* Never read past the end of a shorter default string. */
                const char *src = (const char *)arg->default_val;
                size_t n = 0;

                while(n < arg->size - 1 && src[n])
                    n++;
                memcpy(arg->out_val, src, n);
                ((char *)arg->out_val)[n] = '\0';
            }
            break;

        default:
            memcpy(arg->out_val, arg->default_val, arg->size);
            break;
    }
}

/* Report every required positional/subcommand that was never provided. */
static aparse_error_t aparse__check_required(
        const aparse_argument_t *args,
        const size_t args_count,
        const aparse__scope_t *scope,
        const aparse_config_t *config,
        aparse_result_t *result)
{
    bool missing = false;

    for(size_t i = 0; i < args_count; i++)
    {
        aparse_error_t rc = APARSE_ERR_OK;

        if(args[i].kind != APARSE_ARGUMENT_POSITIONAL && args[i].kind != APARSE_ARGUMENT_SUBPARSERS)
            continue;
        if(scope->states[i].processed_count > 0)
            continue;
        /* A positional with a default value is optional. */
        if(args[i].kind == APARSE_ARGUMENT_POSITIONAL && args[i].default_val)
            continue;

        missing = true;
        result->user_error = APARSE_UERR_MISSING_POSITIONAL;
        rc = aparse__add_string_to_result(result, aparse__positional_name(&args[i]), config);
        if(rc != APARSE_ERR_OK)
        {
            APARSE__ERROR("aparse", "failed to add missing positional argument to result: %s",
                    aparse_strerror(rc));
            return rc;
        }
    }

    return missing ? APARSE_ERR_USER : APARSE_ERR_OK;
}

/* Apply defaults to untouched arguments and publish final array sizes. */
static void aparse__finalize_scope(
        const aparse_argument_t *args,
        const size_t args_count,
        const aparse__scope_t *scope)
{
    for(size_t i = 0; i < args_count; i++)
    {
        if(scope->states[i].processed_count == 0)
            aparse__apply_default(&args[i]);
        if(aparse__arg_is_array(&args[i]) && args[i].out_val)
            ((aparse_array_t *)args[i].out_val)->size = scope->states[i].item_idx;
    }
}

static aparse_error_t aparse__parse_recursive(
        const aparse_argument_t *args,
        const size_t args_count,
        const aparse_config_t *config,
        aparse__persistent_state_t *state,
        aparse_result_t *result)
{
    aparse_error_t rc = APARSE_ERR_OK;
    aparse__scope_t scope = {0};
    bool had_unexpected = false;

    if(!state || !result || !aparse__config_is_valid(config))
        return APARSE_ERR_INVALID_ARGUMENT;
    if(state->depth >= APARSE_MAX_DEPTH)
        return APARSE_ERR_TOO_DEEP;

    rc = aparse__scope_create(args, args_count, config, &scope);
    if(rc != APARSE_ERR_OK)
        return rc;

    state->stack[state->depth++] = (aparse_stack_frame_t){ .args = args, .args_count = args_count };

    while(state->current_idx < state->argc)
    {
        const char *carg = state->argv[state->current_idx++];
        const aparse_argument_t *matched_arg = NULL;

        if(!carg)
            continue;
        matched_arg = aparse__match_arg(&scope, carg, config);

        if(!matched_arg)
        {
            if(state->depth > 1)
            {
                /* Let the parent scope have a go at this argument. */
                state->current_idx--;
                break;
            }

            had_unexpected = true;
            result->user_error = APARSE_UERR_UNEXPECTED_ARGUMENT;
            rc = aparse__add_string_to_result(result, carg, config);
            if(rc != APARSE_ERR_OK)
            {
                APARSE__ERROR("aparse", "failed to add unknown argument to result: %s",
                        aparse_strerror(rc));
                break;
            }
            continue;
        }

        if(aparse__arg_is_special(matched_arg))
        {
            const aparse_stack_frame_t *top = &state->stack[state->depth - 1];

            result->user_error = matched_arg == APARSE__ARG(APARSE__SPECIAL_ACTION_HELP) ?
                APARSE_UERR_HELP : APARSE_UERR_VERSION;
            result->arg = top->args;
            result->arg_count = top->args_count;
            memcpy(result->stack, state->stack, sizeof(result->stack[0]) * state->depth);
            result->stack_frame_count = state->depth;
            rc = APARSE_ERR_USER;
            break;
        }

        switch(matched_arg->kind)
        {
            case APARSE_ARGUMENT_POSITIONAL:
                rc = aparse__process_positional(matched_arg, carg, result);
                if(rc == APARSE_ERR_OK)
                    scope.states[matched_arg - args].processed_count = 1;
                break;

            case APARSE_ARGUMENT_FLAG:
                if(!matched_arg->out_val)
                {
                    rc = APARSE_ERR_INVALID_DESTINATION;
                    break;
                }

                switch(matched_arg->action)
                {
                    case APARSE_ACTION_STORE_TRUE:
                        *(bool *)matched_arg->out_val = true;
                        break;
                    case APARSE_ACTION_STORE_FALSE:
                        *(bool *)matched_arg->out_val = false;
                        break;
                    case APARSE_ACTION_COUNT:
                        rc = aparse__increment_integer(matched_arg);
                        break;
                    default:
                        rc = APARSE_ERR_INVALID_SPEC;
                        break;
                }

                /* Mark as seen so defaults don't overwrite the parsed value. */
                if(rc == APARSE_ERR_OK)
                    scope.states[matched_arg - args].processed_count++;
                break;

            case APARSE_ARGUMENT_OPTION:
                rc = aparse__process_option(matched_arg, carg, config, &scope, state, result);
                if(rc == APARSE_ERR_OK)
                    scope.states[matched_arg - args].processed_count++;
                break;

            case APARSE_ARGUMENT_SUBPARSERS:
            {
                const aparse_argument_t *subcmd = aparse__find_subcommand(matched_arg, carg);

                if(!subcmd)
                {
                    rc = aparse__value_error(result, matched_arg, carg, APARSE_UERR_UNKNOWN_COMMAND);
                    break;
                }

                /* Register the node before descending to keep the list ordered. */
                rc = aparse__append_subcommand(result, subcmd, config);
                if(rc != APARSE_ERR_OK)
                    break;

                rc = aparse__parse_recursive(subcmd->children, subcmd->children_count,
                        config, state, result);
                if(rc != APARSE_ERR_OK)
                    break;

                scope.states[matched_arg - args].processed_count = 1;
                break;
            }

            default:
                rc = APARSE_ERR_INVALID_SPEC;
                break;
        }

        if(rc != APARSE_ERR_OK)
            break;
    }

    /* A stray argument must not be forgotten just because later ones parsed. */
    if(rc == APARSE_ERR_OK && had_unexpected)
        rc = APARSE_ERR_USER;
    if(rc == APARSE_ERR_OK)
        rc = aparse__check_required(args, args_count, &scope, config, result);
    if(rc == APARSE_ERR_OK)
        aparse__finalize_scope(args, args_count, &scope);

    state->depth--;
    aparse__scope_destroy(&scope, config);
    return rc;
}

/* ========================================================================= */
/* MARK: Parsing API                                                         */
/* ========================================================================= */

void aparse_get_program_name(char *out_buf, const size_t size)
{
    if(!out_buf || size == 0)
        return;

    snprintf(out_buf, size, "%s", "unknown");

#if defined(_WIN32) && !defined(__CYGWIN__)
    {
        char path[MAX_PATH] = {0};
        DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
        const char *name = NULL;

        if(len == 0 || len >= MAX_PATH)
            return;
        name = strrchr(path, '\\');
        if(!name)
            name = strrchr(path, '/');
        snprintf(out_buf, size, "%s", name ? name + 1 : path);
    }
#elif defined(__linux__)
    {
        char path[PATH_MAX] = {0};
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        const char *name = NULL;

        if(len <= 0 || (size_t)len >= sizeof(path) - 1)
            return;
        path[len] = '\0';
        name = strrchr(path, '/');
        snprintf(out_buf, size, "%s", name ? name + 1 : path);
    }
#endif
}

aparse_error_t aparse_parse_from_argv(
        const int argc,
        char *const *argv,
        const aparse_argument_t *args,
        const size_t args_count,
        const aparse_config_t *config,
        aparse_result_t *result)
{
    aparse__persistent_state_t persistent = {0};

    if(argc <= 0 || !argv || (!args && args_count > 0) ||
       !aparse__config_is_valid(config) || !result)
        return APARSE_ERR_INVALID_ARGUMENT;

    persistent.argc = (size_t)argc;
    persistent.argv = argv;
    persistent.current_idx = 1;
    memset(result, 0, sizeof(*result));
    return aparse__parse_recursive(args, args_count, config, &persistent, result);
}

aparse_error_t aparse_parse_from_cmdline(
        const char *cmdline,
        const size_t cmdline_len,
        const aparse_argument_t *args,
        const size_t args_count,
        const aparse_config_t *config,
        aparse_result_t *result)
{
    (void)cmdline;
    (void)cmdline_len;
    (void)args;
    (void)args_count;
    (void)config;
    (void)result;
    return APARSE_ERR_UNIMPLEMENTED;
}

/* ========================================================================= */
/* MARK: Output primitives                                                   */
/* ========================================================================= */

/*
 * The primitives below take a FILE *. When it is NULL nothing is written and
 * only the visible width is returned, so one code path both prints and
 * measures. Escape sequences never count towards the width.
 */
static int aparse__puts(FILE *file, const char *str)
{
    if(!str)
        return 0;
    if(file)
        fputs(str, file);
    return (int)strlen(str);
}

static void aparse__set_color(FILE *file, const char *color)
{
    (void)color;
    if(file)
        fputs(APARSE__ANSI_ESCAPE(color), file);
}

/* Print `str` wrapped in `color` followed by a reset. */
static int aparse__put_colored(FILE *file, const char *color, const char *str)
{
    int len = 0;

    aparse__set_color(file, color);
    len = aparse__puts(file, str);
    aparse__set_color(file, APARSE__COLOR_DEFAULT);
    return len;
}

static int aparse__pad(FILE *file, const int count)
{
    if(count <= 0)
        return 0;
    if(file)
        fprintf(file, "%*s", count, "");
    return count;
}

static int aparse__get_terminal_width(void)
{
    /* prioritize using environmental variable to test later */
    {
        const char *env = getenv("COLUMNS");
        if(env && *env)
        {
            char *end = NULL;
            unsigned long width;

            errno = 0;
            width = strtoul(env, &end, 10);
            if(errno == 0 && end != env && *end == '\0' && width > 0 && width <= INT_MAX)
                return (int)width;
        }
    }

#if defined(_WIN32) && !defined(__CYGWIN__)
    {
        int fd = _fileno(APARSE__STREAM);
        if(fd >= 0)
        {
            intptr_t os_handle = _get_osfhandle(fd);
            if(os_handle != -1)
            {
                CONSOLE_SCREEN_BUFFER_INFO info;
                if(GetConsoleScreenBufferInfo((HANDLE)os_handle, &info))
                {
                    int width = info.srWindow.Right - info.srWindow.Left + 1;
                    if(width > 0)
                        return width;
                }
            }
        }
    }
#else
    {
        struct winsize info;
        int fd = fileno(APARSE__STREAM);
        if(fd >= 0 && ioctl(fd, TIOCGWINSZ, &info) == 0 && info.ws_col > 0)
            return (int)info.ws_col;
    }
#endif

    return 80;
}

static void aparse__print_wrapped(const char *text, const int start_col, const int term_width)
{
    const int usable_width = term_width - start_col;
    int col = 0;
    const char *p = text;

    if(!p)
        return;

    while(*p)
    {
        const char *word_start = NULL;
        int word_len = 0;

        if(*p == '\n')
        {
            fputc('\n', APARSE__STREAM);
            aparse__pad(APARSE__STREAM, start_col);
            col = 0;
            p++;
            continue;
        }

        while(*p && isspace((unsigned char)*p) && *p != '\n')
            p++;
        word_start = p;
        while(*p && !isspace((unsigned char)*p))
        {
            p++;
            word_len++;
        }
        if(word_len == 0)
            continue;

        if(col > 0 && col + 1 + word_len > usable_width)
        {
            fputc('\n', APARSE__STREAM);
            aparse__pad(APARSE__STREAM, start_col);
            col = 0;
        }
        if(col > 0)
        {
            fputc(' ', APARSE__STREAM);
            col++;
        }
        fwrite(word_start, 1, (size_t)word_len, APARSE__STREAM);
        col += word_len;
    }
}

/* ========================================================================= */
/* MARK: Output type and default formatting                                  */
/* ========================================================================= */

static APARSE__INLINE const char *aparse__infer_metavar(const aparse_argument_t *arg)
{
    if(!arg)
        return "UNKNOWN";

    switch(arg->type)
    {
        case APARSE_TYPE_STRING: return "STRING";
        case APARSE_TYPE_INT: return "INT";
        case APARSE_TYPE_UINT: return "UINT";
        case APARSE_TYPE_FLOAT: return "FLOAT";
        case APARSE_TYPE_DOUBLE: return "DOUBLE";
        default: return "UNKNOWN";
    }
}

static APARSE__INLINE const char *aparse__arg_type_string(const aparse_argument_t *arg)
{
    if(!arg)
        return "value";

    switch(arg->type)
    {
        case APARSE_TYPE_STRING: return "string";
        case APARSE_TYPE_INT: return "integer";
        case APARSE_TYPE_UINT: return "unsigned integer";
        case APARSE_TYPE_FLOAT: return "floating-point number";
        case APARSE_TYPE_DOUBLE: return "double-precision floating-point number";
        default: return "unknown";
    }
}

#define APARSE__PRINT_DEFAULT_AS(T, FMT) do { \
    T value; \
    memcpy(&value, arg->default_val, sizeof(value)); \
    fprintf(APARSE__STREAM, FMT, value); \
} while(0)

static void aparse__print_default(const aparse_argument_t *arg)
{
    if(!arg || !arg->default_val)
        return;

    switch(arg->type)
    {
        case APARSE_TYPE_STRING:
        {
            const char *str = arg->size == APARSE_STRING_UNBOUNDED ?
                *(const char *const *)arg->default_val : (const char *)arg->default_val;
            fputs(str ? str : "(null)", APARSE__STREAM);
            break;
        }

        case APARSE_TYPE_INT:
            switch(arg->size)
            {
                case 1: APARSE__PRINT_DEFAULT_AS(int8_t, "%" PRId8); break;
                case 2: APARSE__PRINT_DEFAULT_AS(int16_t, "%" PRId16); break;
                case 4: APARSE__PRINT_DEFAULT_AS(int32_t, "%" PRId32); break;
                case 8: APARSE__PRINT_DEFAULT_AS(int64_t, "%" PRId64); break;
                default: fputs("<unknown>", APARSE__STREAM); break;
            }
            break;

        case APARSE_TYPE_UINT:
            switch(arg->size)
            {
                case 1: APARSE__PRINT_DEFAULT_AS(uint8_t, "%" PRIu8); break;
                case 2: APARSE__PRINT_DEFAULT_AS(uint16_t, "%" PRIu16); break;
                case 4: APARSE__PRINT_DEFAULT_AS(uint32_t, "%" PRIu32); break;
                case 8: APARSE__PRINT_DEFAULT_AS(uint64_t, "%" PRIu64); break;
                default: fputs("<unknown>", APARSE__STREAM); break;
            }
            break;

        case APARSE_TYPE_FLOAT:
        {
            float value;
            memcpy(&value, arg->default_val, sizeof(value));
            fprintf(APARSE__STREAM, "%.9g", (double)value);
            break;
        }

        case APARSE_TYPE_DOUBLE:
            APARSE__PRINT_DEFAULT_AS(double, "%.17g");
            break;

        default:
            fputs("<unknown>", APARSE__STREAM);
            break;
    }
}

#undef APARSE__PRINT_DEFAULT_AS

static APARSE__INLINE bool aparse__has_printable_default(const aparse_argument_t *arg)
{
    return arg && arg->default_val &&
           (arg->kind == APARSE_ARGUMENT_POSITIONAL || arg->kind == APARSE_ARGUMENT_OPTION) &&
           !aparse__arg_is_array(arg);
}

/* ========================================================================= */
/* MARK: Usage output                                                        */
/* ========================================================================= */

/* Print one usage argument and return its visible width. */
static int aparse__usage_part(FILE *file, const aparse_argument_t *arg)
{
    int len = 0;

    if(!arg)
        return 0;

    if(aparse__arg_is_optional(arg))
    {
        const bool is_option = arg->kind == APARSE_ARGUMENT_OPTION;
        const char *name = is_option ?
            (arg->longopt ? arg->longopt : arg->shortopt) :
            (arg->shortopt ? arg->shortopt : arg->longopt);

        len += aparse__puts(file, "[");
        aparse__set_color(file, APARSE__COLOR_CYAN);
        len += aparse__puts(file, APARSE__NULL_TO_EMPTY(name));
        if(is_option)
        {
            len += aparse__puts(file, " ");
            aparse__set_color(file, APARSE__COLOR_YELLOW);
            len += aparse__puts(file, arg->metavar ? arg->metavar : aparse__infer_metavar(arg));
        }
        aparse__set_color(file, APARSE__COLOR_DEFAULT);
        len += aparse__puts(file, "]");
    }
    else if(arg->kind == APARSE_ARGUMENT_POSITIONAL)
    {
        len += aparse__put_colored(file, APARSE__COLOR_GREEN,
                APARSE__NULL_TO_EMPTY(aparse__positional_name(arg)));
    }
    else if(arg->kind == APARSE_ARGUMENT_SUBPARSERS)
    {
        if(arg->metavar)
        {
            aparse__set_color(file, APARSE__COLOR_GREEN);
            len += aparse__puts(file, arg->metavar);
            len += aparse__puts(file, " ...");
            aparse__set_color(file, APARSE__COLOR_DEFAULT);
        }
        else if(arg->children && arg->children_count)
        {
            len += aparse__puts(file, "{");
            for(size_t i = 0; i < arg->children_count; i++)
            {
                if(i > 0)
                    len += aparse__puts(file, ", ");
                len += aparse__puts(file, APARSE__NULL_TO_EMPTY(arg->children[i].longopt));
            }
            len += aparse__puts(file, "} ...");
        }
    }

    return len;
}

/* Print one usage argument, folding when necessary. */
static void aparse__print_usage_part(
        FILE *file,
        const int term_width,
        const int indent,
        const aparse_argument_t *arg,
        int *current_col)
{
    int len = 0;

    if(!arg || !current_col)
        return;
    len = aparse__usage_part(NULL, arg);
    if(!len)
        return;

    if(*current_col + 1 + len > term_width)
    {
        fputc('\n', file);
        *current_col = aparse__pad(file, indent);
    }
    else
        *current_col += aparse__puts(file, " ");

    *current_col += aparse__usage_part(file, arg);
}

/* Print the subcommand path from the root to the innermost frame. */
static int aparse__print_command_path(const aparse_result_t *result)
{
    int len = 0;
    size_t idx = 0;

    while(idx + 1 < result->stack_frame_count)
    {
        const aparse_argument_t *args = result->stack[idx].args;
        const size_t args_count = result->stack[idx].args_count;
        const aparse_argument_t *subcmd = NULL;

        if(!args)
            break;

        for(size_t i = 0; i < args_count && !subcmd; i++)
        {
            if(args[i].kind == APARSE_ARGUMENT_POSITIONAL)
            {
                len += aparse__puts(APARSE__STREAM, " ");
                len += aparse__puts(APARSE__STREAM,
                        APARSE__NULL_TO_EMPTY(aparse__positional_name(&args[i])));
                continue;
            }

            subcmd = aparse__find_subcommand_by_scope(&args[i], result->stack[idx + 1].args);
        }
        if(!subcmd)
            break;

        len += aparse__puts(APARSE__STREAM, " ");
        len += aparse__put_colored(APARSE__STREAM, APARSE__COLOR_BOLD_MAGENTA,
                APARSE__NULL_TO_EMPTY(subcmd->longopt));
        idx++;
    }

    return len;
}

/* Print either the optional arguments or the positionals/subparsers. */
static void aparse__print_usage_group(
        const aparse_result_t *result,
        const bool optional,
        const int term_width,
        const int indent,
        int *current_col)
{
    for(size_t i = 0; i < result->arg_count; i++)
    {
        const aparse_argument_t *arg = &result->arg[i];
        if(aparse__arg_is_optional(arg) != optional)
            continue;
        aparse__print_usage_part(APARSE__STREAM, term_width, indent, arg, current_col);
    }
}

void aparse_print_usage(const aparse_result_t *result)
{
    char program_name[APARSE__PROGRAM_NAME_MAX] = {0};
    int term_width = 0;
    int current_col = 0;
    int indent = 0;

    if(!result)
        return;

    aparse_get_program_name(program_name, sizeof(program_name));
    term_width = aparse__get_terminal_width();
    current_col += aparse__put_colored(APARSE__STREAM, APARSE__COLOR_BOLD_BLUE, "usage: ");
    current_col += aparse__put_colored(APARSE__STREAM, APARSE__COLOR_BOLD_MAGENTA, program_name);
    current_col += aparse__puts(APARSE__STREAM, " ");
    current_col += aparse__print_command_path(result);

    /* Arguments fold back to the column after the program and command path. */
    indent = current_col + 1;
    aparse__print_usage_group(result, true, term_width, indent, &current_col);
    aparse__print_usage_group(result, false, term_width, indent, &current_col);
    fputc('\n', APARSE__STREAM);
}

/* ========================================================================= */
/* MARK: Error output                                                        */
/* ========================================================================= */

static void aparse__print_string_list(const char *const *strings, const size_t count)
{
    for(size_t i = 0; i < count; i++)
    {
        if(i > 0)
            fputs(", ", APARSE__STREAM);
        fputs(APARSE__NULL_TO_EMPTY(strings[i]), APARSE__STREAM);
    }
}

void aparse_print_error(const aparse_result_t *result)
{
    char program_name[APARSE__PROGRAM_NAME_MAX] = {0};
    if(!result)
        return;

    aparse_get_program_name(program_name, sizeof(program_name));
    switch(result->user_error)
    {
        case APARSE_UERR_MISSING_VALUE:
            APARSE__ERROR(program_name, "option '%s' expected %zu value%s.",
                    aparse__arg_display_name(result->arg), result->expected_count,
                    result->expected_count == 1 ? "" : "s");
            break;

        case APARSE_UERR_INVALID_VALUE:
            APARSE__ERROR(program_name, "invalid %s '%s'",
                    aparse__arg_type_string(result->arg), APARSE__NULL_TO_EMPTY(result->value));
            break;

        case APARSE_UERR_VALUE_OVERFLOW:
            APARSE__ERROR(program_name, "value '%s' is too large for argument '%s'",
                    APARSE__NULL_TO_EMPTY(result->value), aparse__arg_display_name(result->arg));
            break;

        case APARSE_UERR_VALUE_UNDERFLOW:
            APARSE__ERROR(program_name, "value '%s' underflows argument '%s'",
                    APARSE__NULL_TO_EMPTY(result->value), aparse__arg_display_name(result->arg));
            break;

        case APARSE_UERR_VERIFICATION:
            APARSE__ERROR(program_name, "invalid value '%s' for '%s': %s",
                    APARSE__NULL_TO_EMPTY(result->value), aparse__arg_display_name(result->arg),
                    APARSE__NULL_TO_EMPTY(result->verification_error));
            break;

        case APARSE_UERR_UNKNOWN_COMMAND:
        {
            const aparse_argument_t *children = result->arg->children;
            const size_t count = result->arg->children_count;

            aparse__log_prefix(program_name, APARSE__ERROR_LABEL);
            fprintf(APARSE__STREAM, "invalid choice: '%s' (choose from ",
                    APARSE__NULL_TO_EMPTY(result->value));
            for(size_t i = 0; i < count; i++)
            {
                if(i > 0)
                    fputs(", ", APARSE__STREAM);
                fputs(APARSE__NULL_TO_EMPTY(children[i].longopt), APARSE__STREAM);
            }
            fputs(")\n", APARSE__STREAM);
            break;
        }

        case APARSE_UERR_UNEXPECTED_ARGUMENT:
            aparse__log_prefix(program_name, APARSE__ERROR_LABEL);
            fputs("unrecognized argument: ", APARSE__STREAM);
            aparse__print_string_list(result->strings, result->strings_count);
            fputc('\n', APARSE__STREAM);
            break;

        case APARSE_UERR_MISSING_POSITIONAL:
            aparse__log_prefix(program_name, APARSE__ERROR_LABEL);
            fputs("the following arguments are required: ", APARSE__STREAM);
            aparse__print_string_list(result->strings, result->strings_count);
            fputc('\n', APARSE__STREAM);
            break;

        default:
            APARSE__ERROR(program_name, "%s", aparse_user_strerror(result->user_error));
            break;
    }
}

/* ========================================================================= */
/* MARK: Help output                                                         */
/* ========================================================================= */

/* Print an argument's label (indented) and return its visible width. */
static int aparse__print_argument_label(
        FILE *file,
        const int indent,
        const aparse_argument_t *arg)
{
    int len = 0;

    if(!arg)
        return 0;
    len += aparse__pad(file, indent * APARSE__SPACE_PER_INDENT);

    if(aparse__arg_is_optional(arg))
    {
        const char *name = arg->shortopt ? arg->shortopt : arg->longopt;
        len += aparse__put_colored(file, APARSE__COLOR_BOLD_GREEN, APARSE__NULL_TO_EMPTY(name));

        if(arg->shortopt && arg->longopt)
        {
            len += aparse__puts(file, ", ");
            len += aparse__put_colored(file, APARSE__COLOR_BOLD_CYAN, arg->longopt);
        }
        if(arg->kind == APARSE_ARGUMENT_OPTION)
        {
            len += aparse__puts(file, " ");
            len += aparse__put_colored(file, APARSE__COLOR_BOLD_YELLOW,
                    arg->metavar ? arg->metavar : aparse__infer_metavar(arg));
        }
    }
    else
        len += aparse__put_colored(file, APARSE__COLOR_BOLD_GREEN, aparse__positional_name(arg));

    return len;
}

static void aparse__print_help_text(
        const int label_len,
        const int help_position,
        const aparse_argument_t *arg,
        const int term_width)
{
    if(!arg)
        return;

    if(label_len > help_position)
    {
        fputc('\n', APARSE__STREAM);
        aparse__pad(APARSE__STREAM, help_position + 1);
    }
    else
    {
        aparse__pad(APARSE__STREAM, help_position - label_len);
        fputc(' ', APARSE__STREAM);
    }

    aparse__print_wrapped(APARSE__NULL_TO_EMPTY(arg->help), help_position + 1, term_width);
    if(aparse__has_printable_default(arg))
    {
        fputs(" (default: ", APARSE__STREAM);
        aparse__print_default(arg);
        fputc(')', APARSE__STREAM);
    }
    fputc('\n', APARSE__STREAM);
}

/* Width of the label column: the widest label, capped at the help position. */
static int aparse__help_label_width(const aparse_argument_t *args, const size_t args_count)
{
    int width = 0;

    for(size_t i = 0; i < args_count; i++)
    {
        width = APARSE__MAX(width, aparse__print_argument_label(NULL, 1, &args[i]));
        if(args[i].kind != APARSE_ARGUMENT_SUBPARSERS || !args[i].children)
            continue;

        for(size_t j = 0; j < args[i].children_count; j++)
            width = APARSE__MAX(width, aparse__print_argument_label(NULL, 2, &args[i].children[j]));
    }

    return APARSE__MIN(width, APARSE__DEFAULT_HELP_POSITION);
}

static void aparse__print_help_positionals(
        const aparse_argument_t *args,
        const size_t args_count,
        const int label_width,
        const int term_width)
{
    bool printed = false;

    for(size_t i = 0; i < args_count; i++)
    {
        int label_len = 0;

        if(aparse__arg_is_optional(&args[i]))
            continue;
        if(!printed)
        {
            aparse__put_colored(APARSE__STREAM, APARSE__COLOR_BOLD_BLUE, "positional arguments:");
            fputc('\n', APARSE__STREAM);
            printed = true;
        }

        label_len = aparse__print_argument_label(APARSE__STREAM, 1, &args[i]);
        aparse__print_help_text(label_len, label_width, &args[i], term_width);
        if(args[i].kind != APARSE_ARGUMENT_SUBPARSERS || !args[i].children)
            continue;

        for(size_t j = 0; j < args[i].children_count; j++)
        {
            label_len = aparse__print_argument_label(APARSE__STREAM, 2, &args[i].children[j]);
            aparse__print_help_text(label_len, label_width, &args[i].children[j], term_width);
        }
    }

    if(printed)
        fputc('\n', APARSE__STREAM);
}

static void aparse__print_help_options(
        const aparse_argument_t *args,
        const size_t args_count,
        const int label_width,
        const int term_width)
{
    bool printed = false;

    for(size_t i = 0; i < args_count; i++)
    {
        int label_len = 0;

        if(!aparse__arg_is_optional(&args[i]))
            continue;
        if(!printed)
        {
            aparse__put_colored(APARSE__STREAM, APARSE__COLOR_BOLD_BLUE, "options:");
            fputc('\n', APARSE__STREAM);
            printed = true;
        }

        label_len = aparse__print_argument_label(APARSE__STREAM, 1, &args[i]);
        aparse__print_help_text(label_len, label_width, &args[i], term_width);
    }
}

void aparse_print_help(const aparse_result_t *result)
{
    int term_width = 0;
    int label_width = 0;

    if(!result || !result->arg)
        return;

    term_width = aparse__get_terminal_width();
    label_width = aparse__help_label_width(result->arg, result->arg_count);
    aparse__print_help_positionals(result->arg, result->arg_count, label_width, term_width);
    aparse__print_help_options(result->arg, result->arg_count, label_width, term_width);
}

/* ========================================================================= */
/* MARK: Cleanup API                                                         */
/* ========================================================================= */

void aparse_destroy_array(aparse_array_t *array, const aparse_config_t *config)
{
    if(!array || !config || !config->alloc.free)
        return;
    if(array->data)
        config->alloc.free(config->alloc.userdata, array->data);
    memset(array, 0, sizeof(*array));
}

void aparse_destroy_result(aparse_result_t *result, const aparse_config_t *config)
{
    aparse_subcommand_node_t *head = NULL;

    if(!result || !config || !config->alloc.free)
        return;

    head = result->subcommand_head;
    while(head)
    {
        aparse_subcommand_node_t *next = head->next;
        config->alloc.free(config->alloc.userdata, head);
        head = next;
    }
    if(result->strings)
        config->alloc.free(config->alloc.userdata, result->strings);
    memset(result, 0, sizeof(*result));
}
