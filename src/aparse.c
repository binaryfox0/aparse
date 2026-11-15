
#include <aparse/aparse.h>

#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <limits.h>


#if defined(_WIN32) && !defined(__CYGWIN__)
#    include <windows.h>
#else
#    include <unistd.h>
#endif


/* ========================================================================= */
/* MARK: Platform headers                                                    */
/* ========================================================================= */

#if defined(_WIN32) && !defined(__CYGWIN__)
#    include <io.h>
#   include <Windows.h>
#else
#   include <unistd.h>
#   include <sys/ioctl.h>
#endif

/* ========================================================================= */
/* MARK: Portability macros                                                  */
/* ========================================================================= */

#ifndef APARSE_NO_ANSI_ESCAPE
#    ifdef _WIN32
#        include <sdkddkver.h>
#        if defined(NTDDI_VERSION) && (NTDDI_VERSION >= NTDDI_WIN10_TH2)
#            define APARSE__ANSI_ESCAPE(x) x
#        else
#            define APARSE__ANSI_ESCAPE(x)
#        endif
#    else
#        define APARSE__ANSI_ESCAPE(x) x
#    endif
#else
#    define APARSE__ANSI_ESCAPE(x)
#endif

#if defined(__GNUC__) || defined(__clang__)
#    define APARSE__PRINTF(fmt_index, arg_index) \
        __attribute__((format(printf, fmt_index, arg_index)))
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

#define APARSE__CONST_CAST(type, x) ((type)(uintptr_t)(x))
#define APARSE__PROGRAM_NAME_MAX 256

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
#   if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#       define APARSE__LITTLE_ENDIAN 1
#   else
#       define APARSE__LITTLE_ENDIAN 0
#   endif
#endif

#define APARSE__MAX(a, b) ((a) > (b) ? (a) : (b))
#define APARSE__MIN(a, b) ((a) < (b) ? (a) : (b))

#ifndef APARSE_PRINT_TO_STDOUT
#   define APARSE__STREAM stderr
#else
#   define APARSE__STREAM stdout
#endif

#define APARSE__NARGS_IS_FLEXIBLE(nargs) \
    ((nargs) == APARSE_NARGS_MINIMUM || (nargs) == APARSE_NARGS_MAXIMUM)

/* ========================================================================= */
/* MARK: Logging                                                             */
/* ========================================================================= */

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

#define APARSE__COLORIZE(color, str) \
    APARSE__ANSI_ESCAPE(color) str APARSE__ANSI_ESCAPE(APARSE__COLOR_DEFAULT)

#define APARSE__DEBUG_LABEL APARSE__COLORIZE(APARSE__COLOR_BOLD_CYAN, "debug")
#define APARSE__INFO_LABEL APARSE__COLORIZE(APARSE__COLOR_BOLD_BLUE, "info")
#define APARSE__WARN_LABEL APARSE__COLORIZE(APARSE__COLOR_BOLD_YELLOW, "warn")
#define APARSE__ERROR_LABEL APARSE__COLORIZE(APARSE__COLOR_BOLD_RED, "error")

#define APARSE__DEBUG(source, ...) aparse_log(source, APARSE__DEBUG_LABEL, __VA_ARGS__)
#define APARSE__INFO(source, ...)  aparse_log(source, APARSE__INFO_LABEL, __VA_ARGS__)
#define APARSE__WARN(source, ...)  aparse_log(source, APARSE__WARN_LABEL, __VA_ARGS__)
#define APARSE__ERROR(source, ...) aparse_log(source, APARSE__ERROR_LABEL, __VA_ARGS__)

static APARSE__PRINTF(3, 4) void aparse_log(
        const char *source,
        const char *type,
        APARSE__PRINTF_FMT const char *fmt,
        ...)
{
    va_list va;

    fprintf(APARSE__STREAM, "%s: %s: ", source, type);

    va_start(va, fmt);
    vfprintf(APARSE__STREAM, fmt, va);
    va_end(va);

    fputc('\n', APARSE__STREAM);
}

/* ========================================================================= */
/* MARK: Internal types                                                      */
/* ========================================================================= */

typedef struct
{
    /* number of times that aparse_argument_t was processed */
    size_t processed_count;
    /* index to item of option with nargs, for multiple encounters */
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

#define APARSE__ARG(x) ((const aparse_argument_t*)(uintptr_t)(x))

typedef enum
{
    APARSE__SPECIAL_ACTION_HELP = 1,
    APARSE__SPECIAL_ACTION_VERSION,
} aparse__special_action_t;

/* ========================================================================= */
/* MARK: Allocation utilities                                                */
/* ========================================================================= */

static void *aparse__default_malloc(
        void *userdata,
        size_t size)
{
    const aparse_alloc_t *alloc = (const aparse_alloc_t *)userdata;

    if(!alloc)
        return malloc(size);

    return alloc->malloc ? alloc->malloc(alloc->userdata, size) :
        (alloc->realloc ? alloc->realloc(alloc->userdata, NULL, size) :
         malloc(size));
}

static void *aparse__default_realloc(
        void *userdata,
        void *ptr,
        size_t size)
{
    const aparse_alloc_t *alloc = (const aparse_alloc_t *)userdata;

    if(!alloc)
        return realloc(ptr, size);

    return alloc->realloc ? alloc->realloc(alloc->userdata, ptr, size) :
        realloc(ptr, size);
}

static void aparse__default_free(
        void *userdata,
        void *ptr)
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

/* ========================================================================= */
/* MARK: Error strings                                                       */
/* ========================================================================= */

const char *aparse_strerror(const aparse_error_t err)
{
    static const char *const strings[APARSE__ERR_COUNT] =
    {
        [APARSE_ERR_OK]                         = "success",

        /* API usage errors */
        [APARSE_ERR_UNIMPLEMENTED]              = "operation is not implemented",
        [APARSE_ERR_INVALID_ARGUMENT]           = "invalid argument",

        /* Argument definition errors */
        [APARSE_ERR_INVALID_SPEC]               = "invalid argument specification",
        [APARSE_ERR_DUPLICATE_NAME]             = "duplicate argument name",
        [APARSE_ERR_INVALID_TYPE]               = "invalid argument type",
        [APARSE_ERR_INVALID_SIZE]               = "invalid argument size",
        [APARSE_ERR_INVALID_DESTINATION]        = "invalid argument destination",

        /* Resource errors */
        [APARSE_ERR_OUT_OF_MEMORY]              = "out of memory",
        [APARSE_ERR_SIZE_OVERFLOW]              = "size calculation overflow",

        /* Input/source handling errors */
        [APARSE_ERR_INVALID_CMDLINE]           = "invalid command-line string",
        [APARSE_ERR_INVALID_ENCODING]          = "invalid character encoding",

        /* Internal errors */
        [APARSE_ERR_INTERNAL]                  = "internal error",
    };

    if((int)err < 0 || err >= APARSE__ERR_COUNT)
        return "unknown error";

    return strings[err] ? strings[err] : "unassigned error";
}

const char *aparse_user_strerror(const aparse_user_error_t err)
{
    static const char *const strings[APARSE__UERR_COUNT] =
    {
        [APARSE_UERR_OK]                    = "success",

        /* Special actions */
        [APARSE_UERR_HELP]                  = "user requested help output",
        [APARSE_UERR_VERSION]               = "user requested version output",

        /* Option errors */
        [APARSE_UERR_MISSING_VALUE]         = "option value was not provided",
        [APARSE_UERR_INVALID_VALUE]         = "option value is invalid",
        [APARSE_UERR_VALUE_OVERFLOW]        = "option value exceeds the maximum representable value",
        [APARSE_UERR_VALUE_UNDERFLOW]       = "option value is below the minimum representable value",
        [APARSE_UERR_VERIFICATION]          = "option value failed verification",

        /* Argument errors */
        [APARSE_UERR_UNEXPECTED_ARGUMENT]   = "unexpected command-line argument",
        [APARSE_UERR_MISSING_POSITIONAL]    = "required positional argument was not provided",

        /* Command errors */
        [APARSE_UERR_UNKNOWN_COMMAND]       = "unrecognized subcommand",
        [APARSE_UERR_MISSING_COMMAND]       = "required subcommand was not provided",
    };

    if((int)err < 0 || err >= APARSE__UERR_COUNT)
        return "unknown user error";

    return strings[err] ? strings[err] : "unassigned user error";
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
    if(!out_scope || !args || args_count == 0 || !config)
        return APARSE_ERR_INVALID_ARGUMENT;

    memset(out_scope, 0, sizeof(*out_scope));

    if(args_count > (size_t)-1 / sizeof(*out_scope->states))
        return APARSE_ERR_SIZE_OVERFLOW;

    out_scope->args = args;
    out_scope->args_count = args_count;
    out_scope->states = config->alloc.malloc(
            config->alloc.userdata,
            sizeof(*out_scope->states) * args_count);

    if(!out_scope->states)
    {
        memset(out_scope, 0, sizeof(*out_scope));
        return APARSE_ERR_OUT_OF_MEMORY;
    }

    memset(
            out_scope->states,
            0,
            sizeof(*out_scope->states) * args_count);

    return APARSE_ERR_OK;
}

static APARSE__INLINE aparse__argument_state_t *aparse__scope_get_state(
        aparse__scope_t *scope,
        const aparse_argument_t *arg)
{
    return &scope->states[arg - scope->args];
}

static void aparse__scope_destroy(
        aparse__scope_t *scope,
        const aparse_config_t *config)
{
    if(!scope)
        return;

    if(scope->states)
    {
        config->alloc.free(
                config->alloc.userdata,
                scope->states);
    }

    memset(scope, 0, sizeof(*scope));
}

/* ========================================================================= */
/* MARK: Argument matching                                                   */
/* ========================================================================= */

static APARSE__INLINE bool aparse__match_arg_single(
        const char *shortopt,
        const char *longopt,
        const char *carg)
{
    return (shortopt && strcmp(shortopt, carg) == 0) ||
           (longopt && strcmp(longopt, carg) == 0);
}

static APARSE__INLINE bool aparse__arg_is_special(
        const aparse_argument_t *arg)
{
    return arg == APARSE__ARG(APARSE__SPECIAL_ACTION_HELP) ||
            arg == APARSE__ARG(APARSE__SPECIAL_ACTION_VERSION);
}

static APARSE__INLINE bool aparse__arg_is_positional(
        const aparse_argument_t *arg)
{
    return !aparse__arg_is_special(arg) &&
        (arg->kind == APARSE_ARGUMENT_COMMAND ||
        arg->kind == APARSE_ARGUMENT_POSITIONAL);
}

static APARSE__INLINE const aparse_argument_t *aparse__match_arg(
        aparse__scope_t *scope,
        const char *carg,
        const aparse_config_t *config)
{
    const aparse_argument_t *matched = NULL;
    if(aparse__match_arg_single(config->help.shortopt, config->help.longopt, carg))
        return (const void*)(uintptr_t)APARSE__SPECIAL_ACTION_HELP;
    else if(aparse__match_arg_single(config->version.shortopt, config->version.longopt, carg))
        return (const void*)(uintptr_t)APARSE__SPECIAL_ACTION_VERSION;

    for(size_t i = 0; i < scope->args_count; i++)
    {
        const aparse_argument_t *arg = &scope->args[i];
        if(aparse__arg_is_positional(arg))
        {
            if(scope->states[i].processed_count > 0)
                continue;

            matched = arg;
        }

        if(aparse__match_arg_single(arg->shortopt, arg->longopt, carg))
            return arg;
    }

    return matched;
}

/* ========================================================================= */
/* MARK: Argument processing                                                 */
/* ========================================================================= */

/* Record a user-supplied value that could not be converted. */
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
    size_t bits = 0;

    if(!arg || !carg || !result)
        return APARSE_ERR_INVALID_ARGUMENT;

    if(!arg->out_val)
        return APARSE_ERR_INVALID_DESTINATION;

    if(arg->size == 0 || arg->size > sizeof(uint64_t))
        return APARSE_ERR_INVALID_SIZE;

    if(arg->type != APARSE_TYPE_INT && arg->type != APARSE_TYPE_UINT)
        return APARSE_ERR_INVALID_TYPE;

    is_unsigned = arg->type == APARSE_TYPE_UINT;

    /* Parse the optional sign. */
    if(*p == '-' || *p == '+')
    {
        is_negative = *p == '-';
        p++;
    }

    if(is_negative && is_unsigned)
        return aparse__value_error(
                result, arg, carg, APARSE_UERR_INVALID_VALUE);

    /* Reject empty input and leading whitespace. */
    if(*p == '\0' || isspace((unsigned char)*p))
        return aparse__value_error(
                result, arg, carg, APARSE_UERR_INVALID_VALUE);

    /* Detect an explicit numeric base prefix. */
    if(p[0] == '0' && p[1] != '\0')
    {
        switch(tolower((unsigned char)p[1]))
        {
            case 'x':
                base = 16;
                p += 2;
                break;

            case 'b':
                base = 2;
                p += 2;
                break;

            case 'o':
                base = 8;
                p += 2;
                break;

            default:
                base = 8;
                break;
        }
    }

    errno = 0;
    parsed = strtoull(p, &endptr, base);

    if(endptr == p || *endptr != '\0')
        return aparse__value_error(
                result, arg, carg, APARSE_UERR_INVALID_VALUE);

    if(errno == ERANGE || parsed > (unsigned long long)UINT64_MAX)
    {
        return aparse__value_error(
                result,
                arg,
                carg,
                is_negative
                    ? APARSE_UERR_VALUE_UNDERFLOW
                    : APARSE_UERR_VALUE_OVERFLOW);
    }

    num = (uint64_t)parsed;
    bits = arg->size * 8;

    /* Validate the destination type's representable range. */
    if(is_unsigned)
    {
        const uint64_t max_value = bits == 64
            ? UINT64_MAX
            : ((UINT64_C(1) << bits) - 1);

        if(num > max_value)
            return aparse__value_error(
                    result, arg, carg, APARSE_UERR_VALUE_OVERFLOW);
    }
    else
    {
        const uint64_t sign_bit = UINT64_C(1) << (bits - 1);

        if(is_negative)
        {
            if(num > sign_bit)
                return aparse__value_error(
                        result, arg, carg, APARSE_UERR_VALUE_UNDERFLOW);

            /* Form the two's-complement representation using unsigned math. */
            num = UINT64_C(0) - num;
        }
        else if(num > sign_bit - 1)
        {
            return aparse__value_error(
                    result, arg, carg, APARSE_UERR_VALUE_OVERFLOW);
        }
    }

    /* Copy the low destination-sized bytes in native-endian order. */
    memcpy(
            arg->out_val,
            (const uint8_t *)&num +
                (APARSE__LITTLE_ENDIAN ? 0 : sizeof(num) - arg->size),
            arg->size);

    return APARSE_ERR_OK;
}

static aparse_error_t aparse__process_float(
        const aparse_argument_t *arg,
        const char *carg,
        aparse_result_t *result)
{
    char *endptr = NULL;
    long double value = 0.0L;

    if(!arg || !carg || !result)
        return APARSE_ERR_INVALID_ARGUMENT;

    if(!arg->out_val)
        return APARSE_ERR_INVALID_DESTINATION;

    if(arg->type != APARSE_TYPE_FLOAT && arg->type != APARSE_TYPE_DOUBLE)
        return APARSE_ERR_INVALID_TYPE;

    if((arg->type == APARSE_TYPE_FLOAT && arg->size != sizeof(float)) ||
       (arg->type == APARSE_TYPE_DOUBLE && arg->size != sizeof(double)))
        return APARSE_ERR_INVALID_SIZE;

    /* strtold accepts leading whitespace; this parser deliberately does not. */
    if(*carg == '\0' || isspace((unsigned char)*carg))
        return aparse__value_error(
                result, arg, carg, APARSE_UERR_INVALID_VALUE);

    errno = 0;
    value = strtold(carg, &endptr);

    /* Reject failed conversions and any trailing characters or whitespace. */
    if(endptr == carg || *endptr != '\0')
        return aparse__value_error(
                result, arg, carg, APARSE_UERR_INVALID_VALUE);

    /*
     * A nonzero subnormal long double is still representable and is accepted.
     * A conversion that overflowed to infinity, or underflowed all the way
     * to zero, is reported distinctly.
     */
    if(errno == ERANGE)
    {
        if(isinf(value))
            return aparse__value_error(
                    result, arg, carg, APARSE_UERR_VALUE_OVERFLOW);

        if(value == 0.0L)
            return aparse__value_error(
                    result, arg, carg, APARSE_UERR_VALUE_UNDERFLOW);
    }

    if(arg->type == APARSE_TYPE_FLOAT)
    {
        float converted = 0.0f;

        if(isfinite(value) && fabsl(value) > FLT_MAX)
            return aparse__value_error(
                    result, arg, carg, APARSE_UERR_VALUE_OVERFLOW);

        converted = (float)value;

        if(value != 0.0L && converted == 0.0f)
            return aparse__value_error(
                    result, arg, carg, APARSE_UERR_VALUE_UNDERFLOW);

        *(float *)arg->out_val = converted;
    }
    else
    {
        double converted = 0.0;

        if(isfinite(value) && fabsl(value) > DBL_MAX)
            return aparse__value_error(
                    result, arg, carg, APARSE_UERR_VALUE_OVERFLOW);

        converted = (double)value;

        if(value != 0.0L && converted == 0.0)
            return aparse__value_error(
                    result, arg, carg, APARSE_UERR_VALUE_UNDERFLOW);

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
            if(arg->size == APARSE_STRING_UNBOUNDED)
            {
                *(const char **)arg->out_val = carg;
                return APARSE_ERR_OK;
            }

            if(strlen(carg) > arg->size)
            {
                rc = aparse__value_error(result, arg, 
                    carg, APARSE_UERR_VALUE_OVERFLOW);
                break;
            }
                
            strncpy((char *)arg->out_val, carg, arg->size);
            ((char *)arg->out_val)[arg->size - 1] = '\0';
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
        result->verification_error = arg->verify.verify_fn(
                arg->verify.userdata, arg, arg->out_val);
        if(result->verification_error)
            return aparse__value_error(result, arg, carg, APARSE_UERR_VERIFICATION);
    }

    return rc;
}

static APARSE__INLINE size_t aparse__estimate_elem_size(
        const aparse_argument_t *arg)
{
    return arg->type == APARSE_TYPE_STRING && arg->size == 0 ?
        sizeof(const char*) : arg->size;
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

    new_capacity = array->capacity ? array->capacity :
        APARSE__MIN(4, max_capacity);

    while(new_capacity < size)
    {
        if(new_capacity > max_capacity / 2)
        {
            new_capacity = max_capacity;
            break;
        }

        new_capacity *= 2;
    }

    new_data = config->alloc.realloc(
        config->alloc.userdata,
        array->data,
        new_capacity * elem_size);

    if(!new_data)
        return APARSE_ERR_OUT_OF_MEMORY;

    array->data = new_data;
    array->capacity = new_capacity;

    return APARSE_ERR_OK;
}

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

    if(!arg || !carg || !config || !scope || !persistent || !result)
        return APARSE_ERR_INVALID_ARGUMENT;

    nargs_mode = APARSE_NARGS_MODE(arg->nargs);
    nargs_count = APARSE_NARGS_COUNT(arg->nargs);
    equal = strchr(carg, '=');
    available = equal != NULL;
    
    for(size_t i = persistent->current_idx; i < persistent->argc; i++)
    {
        const aparse_argument_t *matched_arg =
            aparse__match_arg(scope, persistent->argv[i], config);

        if(matched_arg && !aparse__arg_is_positional(matched_arg))
            break;

        available++;
    }

    switch(nargs_mode)
    {
        case APARSE_NARGS_NONE:
            if(available == 0)
                return aparse__value_error(
                    result, arg, NULL, APARSE_UERR_MISSING_VALUE);
            count = 1;
            break;

        case APARSE_NARGS_EXACT:
            if(!nargs_count || available < nargs_count)
                return aparse__value_error(
                    result, arg, NULL, APARSE_UERR_MISSING_VALUE);
            count = nargs_count;
            break;

        case APARSE_NARGS_MINIMUM:
            if(available < nargs_count)
                return aparse__value_error(
                    result, arg, NULL, APARSE_UERR_MISSING_VALUE);
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

    if(!count)
        return aparse__value_error(
            result, arg, NULL, APARSE_UERR_MISSING_VALUE);

    elem_size = aparse__estimate_elem_size(arg);
    state = aparse__scope_get_state(scope, arg);

    if(!state)
        return APARSE_ERR_INVALID_ARGUMENT;

    /*
    * Action       | Nargs          | Storage
    * -------------|----------------|-------------------------
    * STORE        | NONE / EXACT   | Fixed storage (reset)
    * STORE        | MINIMUM / MAX  | Dynamic array (reset)
    * APPEND       | Any            | Dynamic array (preserve)
    */
    if(arg->action == APARSE_ACTION_STORE)
        state->item_idx = 0;

    if(arg->action == APARSE_ACTION_APPEND ||
    (arg->action == APARSE_ACTION_STORE &&
        APARSE__NARGS_IS_FLEXIBLE(nargs_mode)))
    {
        out_arr = arg->out_val;
        required_capacity = state->item_idx + count;

        err = aparse__array_reserve(
            out_arr, elem_size, required_capacity, config);
        if(err != APARSE_ERR_OK)
            return err;

        array_ptr = out_arr->data;
    }
    else if(arg->action == APARSE_ACTION_STORE)
        array_ptr = arg->out_val;

    arg_copy = *arg;

    if(equal)
    {
        arg_copy.out_val =
            (uint8_t *)array_ptr + elem_size * state->item_idx++;

        err = aparse__process_positional(&arg_copy, equal + 1, result);
        if(err != APARSE_ERR_OK)
        {
            // FIX: workaround for UAF
            result->arg = result->arg == &arg_copy ? arg : result->arg;
            return err;
        }

        count--;
    }

    while(count--)
    {
        arg_copy.out_val =
            (uint8_t *)array_ptr + elem_size * state->item_idx++;

        err = aparse__process_positional(
            &arg_copy, persistent->argv[persistent->current_idx++], result);
        if(err != APARSE_ERR_OK)
        {
            result->arg = result->arg == &arg_copy ? arg : result->arg;
            return err;
        }
    }

    return APARSE_ERR_OK;
}
/* ========================================================================= */
/* MARK: Parser internals                                                    */
/* ========================================================================= */

static aparse_error_t aparse__add_string_to_result(
        aparse_result_t *result,
        const char *str,
        const aparse_config_t *config)
{
    if(!result || !str)
        return APARSE_ERR_INVALID_ARGUMENT;

    if(result->strings_count >= result->strings_capacity)
    {
        size_t new_capacity = 0;
        const char **new_strings = NULL;

        new_capacity = result->strings_capacity == 0 ? 4 :
            result->strings_capacity * 2;

        new_strings = config->alloc.realloc(
                config->alloc.userdata,
                result->strings,
                sizeof(*result->strings) * new_capacity);

        if(!new_strings)
            return APARSE_ERR_OUT_OF_MEMORY;

        result->strings = new_strings;
        result->strings_capacity = new_capacity;
    }

    result->strings[result->strings_count++] = str;
    return APARSE_ERR_OK;
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

    if(state->depth >= APARSE_MAX_DEPTH)
        return APARSE_ERR_TOO_DEEP;

    rc = aparse__scope_create(args, args_count, config, &scope);
    if(rc != APARSE_ERR_OK)
        return rc;

    state->stack[state->depth++] = 
    (aparse_stack_frame_t){
        .args = args,
        .args_count = args_count
    };

    while(state->current_idx < state->argc)
    {
        const char *carg = NULL;
        const aparse_argument_t *matched_arg = NULL;

        carg = state->argv[state->current_idx++];
        matched_arg = aparse__match_arg(&scope, carg, config);

        if(!matched_arg)
        {
            aparse_error_t rc2 = APARSE_ERR_OK;
            /* there might be unprocessed argument on higher layer */
            if(state->depth > 1)
            {
                state->current_idx--;
                break;
            }

            rc = APARSE_ERR_USER;
            result->user_error = APARSE_UERR_UNEXPECTED_ARGUMENT;
            rc2 = aparse__add_string_to_result(result, carg, config);
            if(rc2 != APARSE_ERR_OK)
            {
                APARSE__ERROR("aparse", "failed to add unknown argument to result: %s",
                        aparse_strerror(rc));
                break;
            }

            continue;
        }

        if(aparse__arg_is_special(matched_arg))
        {
            result->user_error = matched_arg == APARSE__ARG(APARSE__SPECIAL_ACTION_HELP) ?
                    APARSE_UERR_HELP : APARSE_UERR_VERSION;
            result->arg = state->stack[state->depth - 1].args;
            result->arg_count = state->stack[state->depth - 1].args_count;
            memcpy(
                    result->stack,
                    state->stack,
                    sizeof(result->stack[0]) * state->depth);
            result->stack_frame_count = state->depth;
            rc = APARSE_ERR_USER;
            break;
        }

        switch(matched_arg->kind)
        {
            case APARSE_ARGUMENT_POSITIONAL:
                rc = aparse__process_positional(matched_arg, carg, result);
                if(rc != APARSE_ERR_OK)
                    break;
                scope.states[matched_arg - args].processed_count = 1;
                break;

            case APARSE_ARGUMENT_FLAG:
            {
                if(matched_arg->out_val)
                    *(bool *)matched_arg->out_val = true;
                break;
            }

            case APARSE_ARGUMENT_OPTION:
            {
                rc = aparse__process_option(matched_arg, carg, config, &scope, state, result);
                if(rc != APARSE_ERR_OK)
                    break;
                scope.states[matched_arg - args].processed_count++;
                break;
            }

            case APARSE_ARGUMENT_COMMAND:
            {
                const aparse_argument_t *subcmd = NULL;
                for(size_t i = 0; i < matched_arg->children_count; i++)
                {
                    if(strcmp(carg, matched_arg->children[i].longopt) == 0)
                    {
                        subcmd = &matched_arg->children[i];
                        break;
                    }
                }

                if(!subcmd)
                {
                    result->user_error = APARSE_UERR_UNKNOWN_COMMAND;
                    return APARSE_ERR_USER;
                }

                rc = aparse__parse_recursive(subcmd->children, subcmd->children_count, config, state, result);
                if(rc != APARSE_ERR_OK)
                    break;

                {
                    aparse_subcommand_node_t *node = NULL;
                    aparse_subcommand_node_t **tail = NULL;

                    node = config->alloc.malloc(config->alloc.userdata, sizeof(*node));
                    if(!node)
                        return APARSE_ERR_OUT_OF_MEMORY;

                    memset(node, 0, sizeof(*node));
                    node->command = subcmd->children;

                    tail = &result->subcommand_head;
                    while(*tail)
                        tail = &(*tail)->next;

                    *tail = node;
                }

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

    if(rc == APARSE_ERR_OK)
    {
        for(size_t i = 0; i < args_count; i++)
        {
            aparse_error_t rc2 = APARSE_ERR_OK;
            if(args[i].kind != APARSE_ARGUMENT_POSITIONAL &&
                    args[i].kind != APARSE_ARGUMENT_COMMAND)
                continue;

            if(scope.states[i].processed_count > 0)
                continue;

            rc = APARSE_ERR_USER;
            result->user_error = APARSE_UERR_MISSING_POSITIONAL;
            
            rc2 = aparse__add_string_to_result(result, args[i].longopt, config);
            if(rc2 != APARSE_ERR_OK)
            {
                APARSE__ERROR("aparse", "failed to add missing positional argument to result: %s",
                        aparse_strerror(rc2));
                break;
            }
        }

        for(size_t i = 0; i < args_count; i++)
        {
            if(args[i].action == APARSE_ACTION_APPEND || 
                    APARSE__NARGS_IS_FLEXIBLE(APARSE_NARGS_MODE(args[i].nargs)))
                ((aparse_array_t*)args[i].out_val)->size = scope.states[i].item_idx;
        }
    }

    state->depth--;
    aparse__scope_destroy(&scope, config);
    return rc;
}



/* ========================================================================= */
/* MARK: Parsing API                                                        */
/* ========================================================================= */


void aparse_get_program_name(
        char *out_buf,
        const size_t size)
{
    if(!out_buf || size == 0)
        return;

    snprintf(out_buf, size, "%s", "unknown");

#if defined(_WIN32) && !defined(__CYGWIN__)
    {
        char path[MAX_PATH] = {0};
        DWORD len = 0;

        len = GetModuleFileNameA(NULL, path, MAX_PATH);
        if(len == 0 || len >= MAX_PATH)
            return;

        const char *name = strrchr(path, '\\');
        if(!name)
            name = strrchr(path, '/');

        snprintf(out_buf, size, "%s", name ? name + 1 : path);
    }
#elif defined(__linux__)
    {
        char path[PATH_MAX] = {0};
        const char *name = NULL;

        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
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
    aparse_error_t rc = APARSE_ERR_OK;
    aparse__persistent_state_t persistent = {0};

    if(argc <= 0 || !argv || !args || !config || !result)
        return APARSE_ERR_INVALID_ARGUMENT;

    (void)aparse_log;
    persistent.argc = (size_t)argc;
    persistent.argv = argv;
    persistent.current_idx = 1;

    memset(result, 0, sizeof(*result));

    rc = aparse__parse_recursive(
            args,
            args_count,
            config,
            &persistent,
            result);

    return rc;
}

aparse_error_t aparse_parse_from_cmdline(
        const char *cmdline,
        const size_t cmdline_len,
        const aparse_argument_t *args,
        const size_t args_count,
        const aparse_config_t *config,
        aparse_result_t *result)
{
    return APARSE_ERR_UNIMPLEMENTED;
}


/* ========================================================================= */
/* MARK: Output formatting helpers                                           */
/* ========================================================================= */

static APARSE__INLINE const char *aparse__infer_metavar(
        const aparse_argument_t *arg)
{
    switch(arg->type)
    {
        case APARSE_TYPE_STRING: return "STRING";
        case APARSE_TYPE_INT: return "INT";
        case APARSE_TYPE_UINT: return "UINT";
        case APARSE_TYPE_FLOAT: return "FLOAT";
        case APARSE_TYPE_DOUBLE: return "DOUBLE";
    }
    return "UNKNOWN";
}


static APARSE__INLINE const char *aparse__arg_type_string(
        const aparse_argument_t *arg)
{
    switch(arg->type)
    {
        case APARSE_TYPE_STRING: return "string";
        case APARSE_TYPE_INT: return "integer";
        case APARSE_TYPE_UINT: return "unsigned integer";
        case APARSE_TYPE_FLOAT: return "floating-point number";
        case APARSE_TYPE_DOUBLE: return "double-precision floating-point number";
    }
    return "unknown";
}


static APARSE__PRINTF(2, 3) int aparse__fprintf_safe(
        FILE *file,
        APARSE__PRINTF_FMT const char *fmt, 
        ...)
{
    int len = 0;
    va_list va;

    va_start(va, fmt);
    if(!file)
        len = vsnprintf(NULL, 0, fmt, va);
    else
        len = vfprintf(file, fmt, va);        
    va_end(va);
    return len;
}

#define APARSE__SPACE_PER_INDENT 2
#define APARSE__NULL_TO_EMPTY(x) ((x) ? (x) : "")
static int aparse__print_argument_label(
        FILE *file,
        const int indent,
        const aparse_argument_t *arg)
{
    int len = 0;
    const char *name = NULL;
    const char *metavar = NULL;

    if(!arg)
        return 0;

    len += aparse__fprintf_safe(file, "%*s",
            indent * APARSE__SPACE_PER_INDENT, "");

    if(arg->kind == APARSE_ARGUMENT_FLAG ||
            arg->kind == APARSE_ARGUMENT_OPTION)
    {
        name = arg->shortopt ? arg->shortopt : arg->longopt;

        if(arg->kind == APARSE_ARGUMENT_OPTION)
            metavar = arg->metavar
                ? arg->metavar
                : aparse__infer_metavar(arg);

        if(file)
            aparse__fprintf_safe(file, "%s",
                    APARSE__ANSI_ESCAPE(APARSE__COLOR_BOLD_GREEN));

        len += aparse__fprintf_safe(file, "%s", name ? name : "");

        if(file)
            aparse__fprintf_safe(file, "%s",
                    APARSE__ANSI_ESCAPE(APARSE__COLOR_DEFAULT));

        if(arg->shortopt && arg->longopt)
        {
            len += aparse__fprintf_safe(file, ", ");

            if(file)
                aparse__fprintf_safe(file, "%s",
                        APARSE__ANSI_ESCAPE(APARSE__COLOR_BOLD_CYAN));

            len += aparse__fprintf_safe(file, "%s", arg->longopt);

            if(file)
                aparse__fprintf_safe(file, "%s",
                        APARSE__ANSI_ESCAPE(APARSE__COLOR_DEFAULT));
        }

        if(metavar)
        {
            len += aparse__fprintf_safe(file, " ");

            if(file)
                aparse__fprintf_safe(file, "%s",
                        APARSE__ANSI_ESCAPE(APARSE__COLOR_BOLD_YELLOW));

            len += aparse__fprintf_safe(file, "%s", metavar);

            if(file)
                aparse__fprintf_safe(file, "%s",
                        APARSE__ANSI_ESCAPE(APARSE__COLOR_DEFAULT));
        }
    }
    else
    {
        name = arg->metavar ? arg->metavar : (arg->longopt ? arg->longopt : "");

        if(file)
            aparse__fprintf_safe(file, "%s",
                    APARSE__ANSI_ESCAPE(APARSE__COLOR_BOLD_GREEN));

        len += aparse__fprintf_safe(file, "%s",
                arg->metavar ? arg->metavar : (arg->longopt ? arg->longopt : ""));

        if(file)
            aparse__fprintf_safe(file, "%s",
                    APARSE__ANSI_ESCAPE(APARSE__COLOR_DEFAULT));
    }

    return len;
}

static int aparse__get_terminal_width(void)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    {
        int fd = _fileno(APARSE__STREAM);
        intptr_t os_handle;
        HANDLE handle;
        CONSOLE_SCREEN_BUFFER_INFO info;

        if(fd >= 0)
        {
            os_handle = _get_osfhandle(fd);

            if(os_handle != -1)
            {
                handle = (HANDLE)os_handle;

                if(GetConsoleScreenBufferInfo(handle, &info))
                {
                    int width = info.srWindow.Right -
                        info.srWindow.Left + 1;

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

        if(fd >= 0 &&
                ioctl(fd, TIOCGWINSZ, &info) == 0 &&
                info.ws_col > 0)
            return (int)info.ws_col;
    }

#endif

    {
        const char *env = getenv("COLUMNS");

        if(env && *env)
        {
            char *end = NULL;
            unsigned long width;

            errno = 0;
            width = strtoul(env, &end, 10);

            if(errno == 0 &&
                    end != env &&
                    *end == '\0' &&
                    width > 0 &&
                    width <= INT_MAX)
                return (int)width;
        }
    }

    return 80;
}

static void aparse__print_wrapped(
        const char *text,
        const int start_col,
        const int term_width)
{
    const int usable_width = term_width - start_col;
    int col = 0;
    const char *p = text;

    while(*p)
    {
        const char *word_start;
        int word_len = 0;

        if(*p == '\n')
        {
            fputc('\n', APARSE__STREAM);
            fprintf(APARSE__STREAM, "%*s", start_col, "");
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
            fprintf(APARSE__STREAM, "%*s", start_col, "");
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

#define APARSE__DEFAULT_HELP_POSITION 24

static void aparse__print_help_text(
        const int label_len,
        const int help_position,
        const char *help,
        const int term_width)
{
    if(label_len > help_position)
    {
        fputc('\n', APARSE__STREAM);
        fprintf(APARSE__STREAM, "%*s", help_position + 1, "");
    }
    else
    {
        fprintf(APARSE__STREAM, "%*s ",
                help_position - label_len, "");
    }

    aparse__print_wrapped(
            APARSE__NULL_TO_EMPTY(help),
            help_position + 1,
            term_width);

    fputc('\n', APARSE__STREAM);
}


/* ========================================================================= */
/* MARK: Output API                                                          */
/* ========================================================================= */

void aparse_print_usage(
        const aparse_result_t *result)
{
    char program_name[APARSE__PROGRAM_NAME_MAX] = {0};
    if(!result)
        return;

    /* usage: program_name flags options commands positionals */

    aparse_get_program_name(program_name, sizeof(program_name));
    fprintf(APARSE__STREAM,
            APARSE__COLORIZE(APARSE__COLOR_BOLD_BLUE, "usage:") " "
            APARSE__COLORIZE(APARSE__COLOR_BOLD_MAGENTA, "%s"), 
            program_name);

    /* print from the root to the final stack frame */
    {
        size_t idx = 0;
        for(;;)
        {
            const aparse_argument_t *arg = NULL;
            size_t arg_count = 0;
            bool found = false;

            if(idx >= result->stack_frame_count - 1)
                break;

            arg = result->stack[idx].args;
            for(size_t i = 0; i < arg_count; i++, arg++)
            {
                if(arg->kind != APARSE_ARGUMENT_POSITIONAL &&
                        arg->kind != APARSE_ARGUMENT_COMMAND)
                    continue;

                if(arg->kind == APARSE_ARGUMENT_POSITIONAL)
                {
                    fprintf(APARSE__STREAM, " %s", 
                            arg->metavar ? arg->metavar : arg->longopt);
                    continue;
                }

                for(size_t j = 0; j < arg->children_count; j++)
                {
                    const aparse_argument_t *subcmd = &arg->children[i];
                    if(subcmd->children == result->stack[idx + 1].args)
                    {
                        idx++;
                        found = true;
                        fprintf(APARSE__STREAM, " " APARSE__COLORIZE(APARSE__COLOR_BOLD_MAGENTA, "%s"), 
                                subcmd->longopt);
                        break;
                    }
                }

                if(found)
                    break;
            }

            if(!found)
                break;
        }
    }

    /* print the usage from the final stack frame */
    {
        const aparse_argument_t *arg = result->arg;
        for(size_t i = 0; i < result->arg_count; i++, arg++)
        {
            if(arg->kind != APARSE_ARGUMENT_FLAG &&
                    arg->kind != APARSE_ARGUMENT_OPTION)
                continue;

            if(arg->kind == APARSE_ARGUMENT_FLAG)
            {
                fprintf(APARSE__STREAM,
                        " [" APARSE__COLORIZE(APARSE__COLOR_CYAN, "%s") "]", 
                        arg->shortopt ? arg->shortopt : arg->longopt);
                continue;
            }

            {
                const char *name = arg->longopt ? arg->longopt : arg->shortopt;
                fprintf(APARSE__STREAM, 
                        " [" APARSE__COLORIZE(APARSE__COLOR_CYAN, "%s") "%s"
                        APARSE__COLORIZE(APARSE__COLOR_YELLOW, "%s") "]",
                        name ? name : "",
                        name ? " " : "", 
                        arg->metavar ? arg->metavar : aparse__infer_metavar(arg));
            }
        }

        arg = result->arg;
        for(size_t i = 0; i < result->arg_count; i++, arg++)
        {
            if(arg->kind == APARSE_ARGUMENT_FLAG ||
                    arg->kind == APARSE_ARGUMENT_OPTION)
                continue;

            if(arg->kind == APARSE_ARGUMENT_POSITIONAL)
            {
                fprintf(APARSE__STREAM, " " APARSE__COLORIZE(APARSE__COLOR_GREEN, "%s"), 
                        arg->metavar ? arg->metavar : arg->longopt);
                continue;
            }

            if(arg->metavar)
            {
                fprintf(APARSE__STREAM, " " APARSE__COLORIZE(APARSE__COLOR_GREEN, "%s ..."), 
                        arg->metavar);
                continue;
            }

            if(!arg->children || arg->children_count == 0)
                continue;

            fputs(APARSE__ANSI_ESCAPE(APARSE__COLOR_GREEN) " {", APARSE__STREAM);
            for(size_t j = 0; j < arg->children_count; j++)
            {
                if(j > 0)
                    fputs(", ", APARSE__STREAM);
                fputs(arg->children[j].longopt, APARSE__STREAM);
            }
            fputs("} ..." APARSE__ANSI_ESCAPE(APARSE__COLOR_DEFAULT), APARSE__STREAM);
        }
    }
    fputc('\n', APARSE__STREAM);
}


void aparse_print_error(
        const aparse_result_t *result)
{
    char program_name[APARSE__PROGRAM_NAME_MAX] = {0};
    if(!result)
        return;

    aparse_get_program_name(program_name, sizeof(program_name));
    switch(result->user_error)
    {
        case APARSE_UERR_MISSING_VALUE:
            APARSE__ERROR(program_name, "option '%s' expected %zu argument.",
                    result->arg->longopt ? result->arg->longopt : result->arg->shortopt,
                    result->expected_count);
            break;

        case APARSE_UERR_INVALID_VALUE:
            APARSE__ERROR(program_name, "invalid %s '%s'",
                    aparse__arg_type_string(result->arg), result->value);
            break;

        case APARSE_UERR_VALUE_OVERFLOW:
            APARSE__ERROR(program_name, "value '%s' is too large for argument '%s'",
                    result->value, result->arg->longopt);
            break;

        case APARSE_UERR_VALUE_UNDERFLOW:
            APARSE__ERROR(program_name, "value '%s' underflows argument '%s'",
                    result->value, result->arg->longopt);
            break;

        case APARSE_UERR_VERIFICATION:
            APARSE__ERROR(program_name, "invalid value '%s' for '%s': %s",
                    result->value, result->arg->longopt, result->verification_error);
            break;

        case APARSE_UERR_UNEXPECTED_ARGUMENT:
            fprintf(APARSE__STREAM, "%s: " APARSE__ERROR_LABEL ": unrecognized argument: ",
                    program_name);
            for(size_t i = 0; i < result->strings_count; i++)
            {
                if(i > 0)
                    fprintf(APARSE__STREAM, ", ");
                fprintf(APARSE__STREAM, "%s", result->strings[i]);
            }
            fprintf(APARSE__STREAM, "\n");
            break;

        case APARSE_UERR_MISSING_POSITIONAL:
            fprintf(APARSE__STREAM, "%s: " APARSE__ERROR_LABEL ": the following arguments are required: ",
                    program_name);
            for(size_t i = 0; i < result->strings_count; i++)
            {
                if(i > 0)
                    fprintf(APARSE__STREAM, ", ");
                fprintf(APARSE__STREAM, "%s", result->strings[i]);
            }
            fprintf(APARSE__STREAM, "\n");
            break;

        default:
            APARSE__ERROR(program_name, "%s", aparse_user_strerror(result->user_error));
            break;
    }
}


void aparse_print_help(
        const aparse_result_t *result)
{
    const aparse_argument_t *args = NULL;
    int term_width = 0;
    size_t args_count = 0;
    int label_max_len = 0;

    if(!result)
        return;

    args_count = result->arg_count;
    term_width = aparse__get_terminal_width();

    /* Determine the maximum label length. */
    args = result->arg;

    for(size_t i = 0; i < args_count; i++, args++)
    {
        int label_len = aparse__print_argument_label(NULL, 1, args);

        label_max_len = APARSE__MAX(label_max_len, label_len);

        if(args->kind == APARSE_ARGUMENT_COMMAND &&
                args->children && args->children_count > 0)
        {
            for(size_t j = 0; j < args->children_count; j++)
            {
                label_len = aparse__print_argument_label(
                        NULL, 2, &args->children[j]);

                label_max_len = APARSE__MAX(label_max_len, label_len);
            }
        }
    }

    label_max_len = APARSE__MIN(
            label_max_len,
            APARSE__DEFAULT_HELP_POSITION);

    /* Print positional arguments and commands. */
    {
        bool printed = false;
        int label_len = 0;

        args = result->arg;

        for(size_t i = 0; i < args_count; i++, args++)
        {
            if(args->kind != APARSE_ARGUMENT_POSITIONAL &&
                    args->kind != APARSE_ARGUMENT_COMMAND)
                continue;

            if(!printed)
                fputs(
                        APARSE__COLORIZE(
                            APARSE__COLOR_BOLD_BLUE,
                            "positional arguments:") "\n",
                        APARSE__STREAM);

            label_len = aparse__print_argument_label(
                    APARSE__STREAM, 1, args);

            aparse__print_help_text(
                    label_len,
                    label_max_len,
                    args->help,
                    term_width);

            if(args->kind == APARSE_ARGUMENT_COMMAND &&
                    args->children && args->children_count > 0)
            {
                for(size_t j = 0; j < args->children_count; j++)
                {
                    label_len = aparse__print_argument_label(
                            APARSE__STREAM, 2, &args->children[j]);

                    aparse__print_help_text(
                            label_len,
                            label_max_len,
                            args->children[j].help,
                            term_width);
                }
            }

            printed = true;
        }

        if(printed)
            fputc('\n', APARSE__STREAM);
    }

    /* Print options. */
    {
        bool printed = false;
        int label_len = 0;

        args = result->arg;

        for(size_t i = 0; i < args_count; i++, args++)
        {
            if(args->kind == APARSE_ARGUMENT_POSITIONAL ||
                    args->kind == APARSE_ARGUMENT_COMMAND)
                continue;

            if(!printed)
                fputs(
                        APARSE__COLORIZE(
                            APARSE__COLOR_BOLD_BLUE,
                            "options:") "\n",
                        APARSE__STREAM);

            label_len = aparse__print_argument_label(
                    APARSE__STREAM, 1, args);

            aparse__print_help_text(
                    label_len,
                    label_max_len,
                    args->help,
                    term_width);

            printed = true;
        }
    }
}


/* ========================================================================= */
/* MARK: Cleanup API                                                         */
/* ========================================================================= */

void aparse_destroy_array(
        aparse_array_t *array,
        const aparse_config_t *config)
{
    if(!array || !config)
        return;

    config->alloc.free(config->alloc.userdata, array->data);
    memset(array, 0, sizeof(*array));
}


void aparse_destroy_result(
        aparse_result_t *result,
        const aparse_config_t *config)
{
    aparse_subcommand_node_t *head = NULL;

    if(!result)
        return;

    head = result->subcommand_head;
    while(head)
    {
        aparse_subcommand_node_t *next = head->next;
        config->alloc.free(config->alloc.userdata, head);
        head = next;
    }

    config->alloc.free(config->alloc.userdata, result->strings);
    memset(result, 0, sizeof(*result));
}
