/*
 * MIT License
 * 
 * Copyright (c) 2026 binaryfox0
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "aparse_internal.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================= */
/* MARK: Argument helpers                                                    */
/* ========================================================================= */

#define APARSE__ARG(x) ((const aparse_argument_t *)(uintptr_t)(x))

typedef enum
{
    APARSE__SPECIAL_ACTION_HELP = 1,
    APARSE__SPECIAL_ACTION_VERSION
} aparse__special_action_t;

static APARSE__INLINE bool aparse__arg_is_special(
        const aparse_argument_t *arg)
{
    return arg == APARSE__ARG(APARSE__SPECIAL_ACTION_HELP) ||
           arg == APARSE__ARG(APARSE__SPECIAL_ACTION_VERSION);
}

static APARSE__INLINE bool aparse__arg_is_positional(
        const aparse_argument_t *arg)
{
    return arg && !aparse__arg_is_special(arg) &&
           (arg->kind == APARSE_ARGUMENT_SUBPARSERS || 
            arg->kind == APARSE_ARGUMENT_POSITIONAL);
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
            config->alloc.userdata, 
            sizeof(*out_scope->states) * args_count);
    if(!out_scope->states)
    {
        memset(out_scope, 0, sizeof(*out_scope));
        return APARSE_ERR_OUT_OF_MEMORY;
    }

    memset(out_scope->states, 0, 
            sizeof(*out_scope->states) * args_count);
    return APARSE_ERR_OK;
}

static APARSE__INLINE aparse__argument_state_t *aparse__scope_get_state(
        aparse__scope_t *scope,
        const aparse_argument_t *arg)
{
    if(!scope || !scope->states || !arg || 
            arg < scope->args || 
            arg >= scope->args + scope->args_count)
        return NULL;

    return &scope->states[arg - scope->args];
}

static void aparse__scope_destroy(
        aparse__scope_t *scope, 
        const aparse_config_t *config)
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
    if(aparse__match_arg_single(
                config->help.shortopt, 
                config->help.longopt, carg, 
                false))
        return APARSE__ARG(APARSE__SPECIAL_ACTION_HELP);

    if(aparse__match_arg_single(
                config->version.shortopt, 
                config->version.longopt, carg, 
                false))
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

        if(aparse__match_arg_single(
                    arg->shortopt, arg->longopt, carg,
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
                return aparse__value_error( \
                        result, arg, carg, \
                        APARSE_UERR_VALUE_OVERFLOW); \
            *(utype *)arg->out_val = (utype)num; \
        } \
        else \
        { \
            const uint64_t sign_bit = (uint64_t)(smax) + UINT64_C(1); \
            if(is_negative) \
            { \
                if(num > sign_bit) \
                    return aparse__value_error( \
                            result, arg, carg, \
                            APARSE_UERR_VALUE_UNDERFLOW); \
                *(stype *)arg->out_val = num == sign_bit ? \
                    (stype)(smin) : (stype)-(stype)num; \
            } \
            else \
            { \
                if(num > (uint64_t)(smax)) \
                    return aparse__value_error( \
                            result, arg, carg, \
                            APARSE_UERR_VALUE_OVERFLOW); \
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
        return aparse__value_error(result, arg, carg, 
                APARSE_UERR_INVALID_VALUE);
    if(*p == '\0' || *p == '+' || *p == '-' || isspace((unsigned char)*p))
        return aparse__value_error(result, arg, carg, 
                APARSE_UERR_INVALID_VALUE);

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
        return aparse__value_error(result, arg, carg, 
                APARSE_UERR_INVALID_VALUE);

    errno = 0;
    parsed = strtoull(p, &endptr, base);
    if(endptr == p || *endptr != '\0')
        return aparse__value_error(result, arg, carg, 
                APARSE_UERR_INVALID_VALUE);
    if(errno == ERANGE || parsed > (unsigned long long)UINT64_MAX)
        return aparse__value_error(result, arg, carg, is_negative ? 
                APARSE_UERR_VALUE_UNDERFLOW : APARSE_UERR_VALUE_OVERFLOW);

    num = (uint64_t)parsed;
    switch(arg->size)
    {
        case sizeof(uint8_t):
            APARSE__STORE_INTEGER(uint8_t, int8_t, 
                    UINT8_MAX, INT8_MIN, INT8_MAX);
        case sizeof(uint16_t):
            APARSE__STORE_INTEGER(uint16_t, int16_t, 
                    UINT16_MAX, INT16_MIN, INT16_MAX);
        case sizeof(uint32_t):
            APARSE__STORE_INTEGER(uint32_t, int32_t, 
                    UINT32_MAX, INT32_MIN, INT32_MAX);
        case sizeof(uint64_t):
            APARSE__STORE_INTEGER(uint64_t, int64_t, 
                    UINT64_MAX, INT64_MIN, INT64_MAX);
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
        return aparse__value_error(result, arg, carg, 
                APARSE_UERR_INVALID_VALUE);

    errno = 0;
    value = strtold(carg, &endptr);
    if(endptr == carg || *endptr != '\0')
        return aparse__value_error(result, arg, carg, 
                APARSE_UERR_INVALID_VALUE);
    if(errno == ERANGE)
    {
        if(isinf(value))
            return aparse__value_error(result, arg, carg, 
                    APARSE_UERR_VALUE_OVERFLOW);
        if(value == 0.0L)
            return aparse__value_error(result, arg, carg, 
                    APARSE_UERR_VALUE_UNDERFLOW);
    }

    max_value = arg->type == APARSE_TYPE_FLOAT ? 
        (long double)FLT_MAX : (long double)DBL_MAX;
    if(isfinite(value) && fabsl(value) > max_value)
        return aparse__value_error(result, arg, carg, 
                APARSE_UERR_VALUE_OVERFLOW);

    if(arg->type == APARSE_TYPE_FLOAT)
    {
        float converted = (float)value;
        if(value != 0.0L && converted == 0.0f)
            return aparse__value_error(result, arg, carg, 
                    APARSE_UERR_VALUE_UNDERFLOW);
        *(float *)arg->out_val = converted;
    }
    else
    {
        double converted = (double)value;
        if(value != 0.0L && converted == 0.0)
            return aparse__value_error(result, arg, carg, 
                    APARSE_UERR_VALUE_UNDERFLOW);
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
                break;
            }

            /* The buffer must hold the string plus its terminating NUL. */
            len = strlen(carg);
            if(len >= arg->size)
            {
                rc = aparse__value_error(result, arg, carg, 
                        APARSE_UERR_VALUE_OVERFLOW);
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
        result->verification_error = arg->verify.verify_fn(
                arg->verify.userdata, arg, arg->out_val);
        if(result->verification_error)
            return aparse__value_error(result, arg, carg, 
                    APARSE_UERR_VERIFICATION);
    }

    return rc;
}

/* ========================================================================= */
/* MARK: Array storage                                                       */
/* ========================================================================= */

static APARSE__INLINE size_t aparse__estimate_elem_size(
        const aparse_argument_t *arg)
{
    return arg->type == APARSE_TYPE_STRING && 
        arg->size == APARSE_STRING_UNBOUNDED ?
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

    new_capacity = array->capacity ? 
        array->capacity : APARSE__MIN(4, max_capacity);
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

static aparse_error_t aparse__array_append(
        aparse_array_t *array,
        const size_t elem_size,
        const void *elem,
        const aparse_config_t *config)
{
    aparse_error_t rc = aparse__array_reserve(
            array, elem_size, array->size + 1, config);
    if(rc != APARSE_ERR_OK)
        return rc;

    memcpy((uint8_t*)array->data + elem_size * array->size, elem, elem_size);
    array->size++;
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

    if(!arg || !carg || !aparse__config_is_valid(config) || 
            !scope || !persistent || !result)
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
                return aparse__value_error(result, arg, NULL, 
                        APARSE_UERR_MISSING_VALUE);
            count = 1;
            break;

        case APARSE_NARGS_EXACT:
            if(!nargs_count)
                return APARSE_ERR_INVALID_SPEC;
            result->expected_count = nargs_count;
            if(available < nargs_count)
                return aparse__value_error(result, arg, NULL, 
                        APARSE_UERR_MISSING_VALUE);
            count = nargs_count;
            break;

        case APARSE_NARGS_MINIMUM:
            result->expected_count = nargs_count;
            if(available < nargs_count)
                return aparse__value_error(result, arg, NULL, 
                        APARSE_UERR_MISSING_VALUE);
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
        err = aparse__array_reserve(out_arr, elem_size, 
                required_capacity, config);
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
        if(persistent->current_idx >= persistent->argc || 
                !persistent->argv[persistent->current_idx])
            return APARSE_ERR_INTERNAL;

        arg_copy.out_val = (uint8_t *)array_ptr + elem_size * state->item_idx++;
        err = aparse__process_positional(
                &arg_copy, persistent->argv[persistent->current_idx++], 
                result);
        if(err != APARSE_ERR_OK)
        {
            result->arg = result->arg == &arg_copy ? arg : result->arg;
            return err;
        }
    }

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

    node = config->alloc.malloc(
            config->alloc.userdata, sizeof(*node));
    if(!node)
        return APARSE_ERR_OUT_OF_MEMORY;

    memset(node, 0, sizeof(*node));
    node->command = subcmd;
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
                *(const char **)arg->out_val = 
                    *(const char *const *)arg->default_val;
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
        const char *name = NULL;

        if(args[i].kind != APARSE_ARGUMENT_POSITIONAL && 
                args[i].kind != APARSE_ARGUMENT_SUBPARSERS)
            continue;
        if(scope->states[i].processed_count > 0)
            continue;
        /* A positional with a default value is optional. */
        if(args[i].kind == APARSE_ARGUMENT_POSITIONAL && args[i].default_val)
            continue;

        missing = true;
        result->user_error = APARSE_UERR_MISSING_POSITIONAL;
        name = aparse__positional_name(&args[i]);
        rc = aparse__array_append(
                &result->strings, sizeof(const char*),
                &name, config);
        if(rc != APARSE_ERR_OK)
        {
            APARSE__ERROR("aparse", 
                    "failed to add missing positional argument to result: %s",
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

    state->stack[state->depth++] = (aparse_stack_frame_t){ 
        .args = args, 
        .args_count = args_count 
    };

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
            rc = aparse__array_append(
                    &result->strings, sizeof(const char*),
                    &carg, config);
            if(rc != APARSE_ERR_OK)
            {
                APARSE__ERROR("aparse", 
                        "failed to add unknown argument to result: %s",
                        aparse_strerror(rc));
                break;
            }
            continue;
        }

        if(aparse__arg_is_special(matched_arg))
        {
            const aparse_stack_frame_t *top = &state->stack[state->depth - 1];

            result->user_error = matched_arg == 
                APARSE__ARG(APARSE__SPECIAL_ACTION_HELP) ?
                    APARSE_UERR_HELP : APARSE_UERR_VERSION;
            result->arg = top->args;
            result->arg_count = top->args_count;
            memcpy(result->stack, state->stack, 
                    sizeof(result->stack[0]) * state->depth);
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
                rc = aparse__process_option(matched_arg, carg, config, 
                        &scope, state, result);
                if(rc == APARSE_ERR_OK)
                    scope.states[matched_arg - args].processed_count++;
                break;

            case APARSE_ARGUMENT_SUBPARSERS:
            {
                const aparse_argument_t *subcmd = 
                    aparse__find_subcommand(matched_arg, carg);

                if(!subcmd)
                {
                    rc = aparse__value_error(
                            result, matched_arg, 
                            carg, APARSE_UERR_UNKNOWN_COMMAND);
                    break;
                }
                
                rc = aparse__append_subcommand(result, subcmd, config);
                if(rc != APARSE_ERR_OK)
                    break;


                rc = aparse__parse_recursive(
                        subcmd->children, 
                        subcmd->children_count,
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
        rc = aparse__check_required(args, args_count, 
                &scope, config, result);
    if(rc == APARSE_ERR_OK)
        aparse__finalize_scope(args, args_count, &scope);

    state->depth--;
    aparse__scope_destroy(&scope, config);
    return rc;
}

/* ========================================================================= */
/* MARK: Parsing API                                                         */
/* ========================================================================= */


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
    return aparse__parse_recursive(args, args_count, 
            config, &persistent, result);
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
