#include "aparse_internal.h"

static aparse_status aparse__process_argument(
    const char* argv,
    const aparse_arg* arg,
    aparse__context_t *ctx);

static aparse_status aparse__process_parser(
    const int argc,
    const char* cargv,
    char* const* argv,
    aparse_arg* arg,
    aparse__context_t* ctx);

static aparse_status aparse_process_optional(
    const int argc,
    char* const* argv,
    aparse_arg* arg,
    aparse__context_t *ctx);

static aparse_status aparse__process_float(
    const char* argv,
    const aparse_arg* arg);

static aparse_status aparse__process_array(
    const int argc,
    char* const* argv,
    aparse_arg* arg,
    aparse__context_t *ctx);

static size_t aparse__eval_size(
    const aparse_arg* arg);

static bool aparse__verify_layout(
    const aparse_arg *arg,
    int *invalid_idx);

static int aparse__fill_args_dest(
    const aparse_arg* arg,
    uint8_t *buffer);

static void aparse__destroy_payload(
    const aparse_arg* args,
    uint8_t *payload);

static void aparse__release_dispatch_entry(
    aparse__dispatch_t *entry);

const aparse_arg aparse__help_arg = 
{ 
    .shortopt = "-h", 
    .longopt = "--help", 
    .help = "show this help message and exit", 
    .type = APARSE_ARG_TYPE_BOOL 
};

static void aparse__release_dispatch_entry(
        aparse__dispatch_t *entry)
{
    if(entry->args)
        aparse__destroy_payload(entry->args, entry->payload);
    if(entry->payload && entry->freeable)
        free(entry->payload);
    entry->payload = NULL;
}

void aparse_dispatch_all(
        aparse_list* dispatch_list)
{
    if(!dispatch_list)
        return;
    if(dispatch_list->ptr &&
            dispatch_list->itemsz != sizeof(aparse__dispatch_t))
        return;

    for(size_t i = 0; i < dispatch_list->size; i++)
    {
        aparse__dispatch_t *entry =
            &aparse_list_get(dispatch_list, aparse__dispatch_t, i);

        if(entry->args && entry->args->handler)
            entry->args->handler(entry->payload);
        aparse__release_dispatch_entry(entry);
    }
    aparse_list_free(dispatch_list);
}

void aparse_dispatch_free(
        aparse_list* dispatch_list)
{
    if(!dispatch_list)
        return;
    if(dispatch_list->ptr &&
            dispatch_list->itemsz != sizeof(aparse__dispatch_t))
        return;

    aparse__dispatch_t* list = dispatch_list->ptr;
    for(size_t i = 0; i < dispatch_list->size; i++)
        aparse__release_dispatch_entry(&list[i]);
    aparse_list_free(dispatch_list);
}

aparse_status aparse__parse_impl(
        const int argc, 
        char* const * argv, 
        aparse_arg* args, 
        aparse__context_t* ctx)
{
    int *idx = &ctx->idx;
    if(!args)
        return APARSE_STATUS_OK;

    if(ctx->stack_top >= APARSE__MAX_DEPTH)
        return APARSE_STATUS_TOO_DEEP;
    
    ctx->stack[ctx->stack_top++] = args;
    while (*idx < argc) {
        const char* cargv = argv[*idx];
        (*idx)++;

        aparse_arg* ptr = aparse__argv_match(cargv, args);
        if(ptr) {
            if(aparse__is_positional(ptr)) 
            {
                ptr->flags |= APARSE__ARG_PROCESSED;
                if(aparse__is_argument(ptr)) 
                {
                    aparse_status status = APARSE_STATUS_OK;
                    if(ptr->type & APARSE_ARG_TYPE_ARRAY)
                    {
                        status = aparse__process_array(
                                argc, 
                                argv, 
                                ptr, 
                                ctx);
                    } else {
                        status = aparse__process_argument(
                                cargv, 
                                ptr, 
                                ctx);
                    }

                    if(status != APARSE_STATUS_OK)
                        return status;
                } else  {
                    aparse_status status = aparse__process_parser(argc, cargv, argv, ptr, ctx);
                    if(status != APARSE_STATUS_OK)
                        return status;
                    ctx->stack_top--;
                }
            } else {
                if(ptr->shortopt != aparse__help_arg.shortopt) {
                    aparse_status status = aparse_process_optional(
                                argc, 
                                argv, 
                                ptr, 
                                ctx);
                    if(status != APARSE_STATUS_OK)
                        return status;
                } else {
                    aparse__print_help(args, ctx);
                    return APARSE_STATUS_FAILURE;
                }
            }
        } else {
            if (ctx->stack_top > 1) {
                (*idx)--; 
                break;
            }

            if (!aparse_list_add(ctx->unknown, &cargv)) 
            {
                aparse__raise_fatal(ctx, APARSE_STATUS_ALLOC_FAILURE, 
                        NULL, NULL);
            }
        }
    }

    return APARSE_STATUS_OK;
}

static aparse_status aparse__process_argument(
        const char* argv, 
        const aparse_arg *arg,
        aparse__context_t *ctx) 
{
    if (!arg->ptr)
    {
        aparse__raise_nonfatal(ctx, APARSE_STATUS_NULL_POINTER, arg, NULL);
        return APARSE_STATUS_OK;
    }
    if (arg->size == 0 &&
            (arg->type & APARSE_ARG_TYPE_BITMASK) != APARSE_ARG_TYPE_STRING)
        aparse__raise_fatal(ctx, APARSE_STATUS_INVALID_SIZE, arg, &arg->size);

    switch(arg->type & APARSE_ARG_TYPE_BITMASK)
    {
        case APARSE_ARG_TYPE_STRING:
        {
            size_t len = strlen(argv) + 1;

            if (arg->size == 0) 
            {
                const char** dst = (const char**)arg->ptr;
                *dst = argv;
            } else {
                size_t n = APARSE__MIN(arg->size - 1, len);
                memcpy(arg->ptr, argv, n);
                ((char*)arg->ptr)[n] = '\0';
            }
            break;
        }
        case APARSE_ARG_TYPE_UNSIGNED:
        {
            bool have_sign = arg->type & APARSE_ARG_TYPE_SIGNED_FLAGS;
            if(arg->size > sizeof(uint64_t))
                aparse__raise_fatal(ctx, APARSE_STATUS_UNHANDLED, arg, 0);

            int base = 10;
            const char* p = argv;
            bool is_negative = false;

            if (*p == '-' || *p == '+') {
                if (*p == '-') is_negative = true;
                p++;
            }
            if(is_negative && !have_sign)
                aparse__raise_fatal(ctx, APARSE_STATUS_UNDERFLOW, arg, 0);

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
                aparse__raise_fatal(ctx, APARSE_STATUS_INVALID_VALUE, arg, argv);
     
            if(errno == ERANGE)
            {
                if(num == ULLONG_MAX || (int64_t)num == LLONG_MAX)
                    aparse__raise_fatal(ctx, APARSE_STATUS_OVERFLOW, arg, argv);
                if((int64_t)num == LLONG_MIN)
                    aparse__raise_fatal(ctx, APARSE_STATUS_UNDERFLOW, arg, argv);
            } else {
                size_t bits = arg->size * 8;
                if (have_sign) {
                    if(is_negative)
                    {
                        if(num > (uint64_t)INT64_MAX + 1ULL)
                            aparse__raise_fatal(ctx, APARSE_STATUS_UNDERFLOW, arg, argv);
                        if(num == (uint64_t)INT64_MAX + 1ULL)
                            num = (uint64_t)INT64_MIN;
                        else
                            num = (uint64_t) -(int64_t)num;
                    } else {
                        int64_t min_val = 0;
                        int64_t max_val = 0;

                        if(num > (uint64_t)INT64_MAX)
                            aparse__raise_fatal(ctx, APARSE_STATUS_OVERFLOW, arg, argv);
                        min_val = bits == 64 ? INT64_MIN : -(1LL << (bits - 1));
                        max_val = bits == 64 ? INT64_MAX :  (1LL << (bits - 1)) - 1;
                        if ((int64_t)num < min_val || (int64_t)num > max_val)
                            aparse__raise_fatal(ctx, APARSE_STATUS_OVERFLOW, arg, argv);
                    }
                } else {
                    uint64_t max_val = bits == 64 ? UINT64_MAX : ((1ULL << bits) - 1);
                    if (num > max_val)
                        aparse__raise_fatal(ctx, APARSE_STATUS_OVERFLOW, arg, argv);
                }
            }

            memcpy(arg->ptr, 
                    (uint8_t*)&num + (APARSE_IS_LE ? 0 : 8 - arg->size), arg->size);

            if (have_sign) {
                uint8_t* msb = (uint8_t*)arg->ptr + 
                    (APARSE_IS_LE ? arg->size - 1 : 0);
                *msb = (uint8_t)((*msb & ~0x80) | (((int64_t)num < 0) ? 0x80 : 0));
            }
            break;
        }
        case APARSE_ARG_TYPE_FLOAT:
        {
            aparse_status status = aparse__process_float(argv, arg);
            if(status != APARSE_STATUS_OK)
            {
                if(status != APARSE_STATUS_UNDERFLOW)
                    aparse__raise_fatal(ctx, status, arg, argv)
                else
                    aparse__raise_nonfatal(ctx, status, arg, argv);
            }
            break;
        }
        default:
            aparse__raise_fatal(ctx, APARSE_STATUS_INVALID_TYPE, arg, 0);
    }
    return APARSE_STATUS_OK;
}

static aparse_status aparse__process_parser(
        const int argc,
        const char* cargv,
        char* const* argv,
        aparse_arg* arg,
        aparse__context_t* ctx)
{
    aparse_status ret = APARSE_STATUS_OK;
    aparse_arg* subparser = 0;
    uint8_t* buffer = 0;
    int invalid_idx = 0;
    size_t min_size = 0;
    size_t last_idx = 0;
    bool freeable = false;

    if(!arg->subargs)
    {
        aparse__raise_nonfatal(ctx,
                APARSE_STATUS_NULL_POINTER, arg, NULL);
        return APARSE_STATUS_OK;
    }

    aparse__foreach(item, arg)
    {
        if(!strcmp(cargv, item->longopt))
        {
            subparser = item;
            break;
        }
    }

    if(!subparser)
    {
        aparse_list arg_list = { .ptr = (void*)arg->subargs };

        for(aparse_arg* copy = arg_list.ptr;
            aparse_arg_nend(copy);
            copy++)
        {
            arg_list.size++;
        }

        aparse__raise_fatal(ctx,
                APARSE_STATUS_INVALID_SUBCOMMAND,
                &arg_list, cargv);
    }

    if(!subparser->subargs)
    {
        aparse_list_add(ctx->dispatch,
                (aparse__dispatch_t[1]){{
                    .args = subparser,
                    .payload = subparser->ptr
                }});
        return APARSE_STATUS_OK;
    }

    if(subparser->layout_size != 0)
    {
        last_idx = subparser->layout_size - 1;
        if(!aparse__verify_layout(subparser, &invalid_idx))
        {
            aparse__raise_fatal(ctx,
                    APARSE_STATUS_INVALID_LAYOUT,
                    subparser, &invalid_idx);
        }

        min_size =
                subparser->data_layout[last_idx * 2] +
                subparser->data_layout[last_idx * 2 + 1];
        if(subparser->ptr)
        {
            if(subparser->size < min_size)
            {
                aparse__raise_fatal(ctx,
                        APARSE_STATUS_INVALID_SIZE,
                        subparser, &subparser->size);
            }
            buffer = subparser->ptr;
        }
        else
        {
            buffer = calloc(min_size, sizeof(*buffer));
            if(!buffer)
            {
                aparse__raise_fatal(ctx,
                        APARSE_STATUS_ALLOC_FAILURE, 0, 0);
            }
            freeable = true;
        }
    }

    aparse__fill_args_dest(subparser, buffer);
    ret = aparse__parse_impl(
            argc, argv, subparser->subargs, ctx);
    if(ret == APARSE_STATUS_OK)
        ret = aparse__check_missing(ctx, subparser->subargs);

    if(!subparser->handler || ret != APARSE_STATUS_OK)
    {
        if(freeable)
            free(buffer);
        return ret;
    }

    aparse_list_add(ctx->dispatch,
            (aparse__dispatch_t[1]){{
                .args = subparser,
                .payload = buffer,
                .freeable = freeable
            }});
    return ret;
}

static aparse_status aparse_process_optional(
        const int argc, 
        char* const* argv, 
        aparse_arg* arg,
        aparse__context_t *ctx)
{
    int *idx = &ctx->idx;
    if(aparse__type_cmp(arg, APARSE_ARG_TYPE_BOOL))
    {
        bool was_set = false;
        uint8_t *lsb = NULL;

        if(arg->flags & APARSE__ARG_PROCESSED)
            return APARSE_STATUS_FAILURE;
        for(size_t i = 0; i < arg->size; i++)
        {
            if(((char*)arg->ptr)[i])
                was_set = true;
        }

        memset(arg->ptr, 0, arg->size);
        lsb = (uint8_t*)arg->ptr + (APARSE_IS_LE ? 0 : arg->size - 1);
        if(!was_set) 
            *lsb |= 1;
        arg->flags |= APARSE__ARG_PROCESSED;
        return APARSE_STATUS_OK;
    }
    if(arg->flags & APARSE__ARG_EQUAL_VAL) 
    {
        const char* optname = (arg->flags & APARSE__ARG_SHORT_MATCH) ? 
            arg->shortopt : arg->longopt;
        return aparse__process_argument(
                argv[*idx - 1] + 1 + strlen(optname), 
                arg,
                ctx);
    }
    else {
        if(*idx >= argc) {
            int expected_count = 1;
            aparse__raise_fatal(ctx, APARSE_STATUS_MISSING_VALUE, arg, &expected_count);
        }
        (*idx)++;
        return aparse__process_argument(
                argv[*idx - 1], 
                arg, 
                ctx);
    }
}

static aparse_status aparse__process_float(
        const char* argv, 
        const aparse_arg* arg)
{
    char* endptr = 0;
    errno = 0;
    long double num = strtold(argv, &endptr);
    long double abs_val = fabsl(num);

    if(*endptr)
        return APARSE_STATUS_INVALID_VALUE;
    
    if(errno == ERANGE)
    {
        if((num == HUGE_VALL || num == -HUGE_VALL))
            return APARSE_STATUS_OVERFLOW;
        if(num == 0.0 || abs_val < LDBL_MIN)
            return APARSE_STATUS_UNDERFLOW;
    }

    switch (arg->size) 
    {
        case sizeof(float):
            if (abs_val > FLT_MAX && abs_val != INFINITY)
                return APARSE_STATUS_OVERFLOW;
            if (num != 0.0L && abs_val < FLT_MIN)
                return APARSE_STATUS_UNDERFLOW;
            *(float *)arg->ptr = (float)num;
            break;

        case sizeof(double):
            if (abs_val > DBL_MAX && abs_val != INFINITY)
                return APARSE_STATUS_OVERFLOW;
            if (num != 0.0L && abs_val < DBL_MIN)
                return APARSE_STATUS_UNDERFLOW;
            *(double *)arg->ptr = (double)num;
            break;

        case sizeof(long double):
            *(long double *)arg->ptr = num;
            break;

        default:
            return APARSE_STATUS_UNHANDLED;
    }
    return 0;
}

static aparse_status aparse__process_array(
        const int argc, 
        char* const * argv, 
        aparse_arg* arg, 
        aparse__context_t *ctx)
{
    int *idx = &ctx->idx;
    aparse_list* dest = arg->ptr;
    size_t arrsz = 0, increment = 0, remaining = 0, reserved = 0;

    if(!dest)
        aparse__raise_fatal(ctx, APARSE_STATUS_NULL_POINTER, arg, 0);
    if(arg->size < sizeof(aparse_list))
        aparse__raise_fatal(ctx, APARSE_STATUS_INVALID_SIZE, arg, &arg->size);
    if((arg->type & APARSE_ARG_TYPE_BITMASK) == APARSE_ARG_TYPE_UNKNOWN)
        aparse__raise_fatal(ctx, APARSE_STATUS_INVALID_TYPE, arg, 0);

    (*idx)--;
    remaining = (size_t)(argc - *idx);
    for (aparse_arg* next = arg + 1; aparse_arg_nend(next); next++)
    {
        size_t required = 0;
        if (!aparse__is_positional(next))
            continue;
        required = next->type & APARSE_ARG_TYPE_ARRAY ? next->array_size : 1;
        if (reserved >= remaining || required >= remaining - reserved)
        {
            reserved = remaining;
            break;
        }
        reserved += required;
    }
    arrsz = remaining - reserved;
    increment = arg->type & APARSE_ARG_TYPE_STRING ? 
        sizeof(char*) : 
        arg->element_size;

    if(increment <= 0)
        aparse__raise_fatal(ctx, APARSE_STATUS_INVALID_SIZE, arg, &arg->element_size);

    if(arrsz < arg->array_size)
        aparse__raise_fatal(ctx, APARSE_STATUS_MISSING_VALUE, arg, &arrsz);
    
    if(!aparse_list_new(dest,  arrsz, increment))
        aparse__raise_fatal(ctx, APARSE_STATUS_ALLOC_FAILURE, 0, 0);
    arg->ptr = dest->ptr;
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
        arg->ptr = (uint8_t*)arg->ptr + increment;
    }
    arg->ptr = dest;
    return APARSE_STATUS_OK;
}

static size_t aparse__eval_size(
        const aparse_arg* arg)
{
    if(arg->type & APARSE_ARG_TYPE_ARRAY)
        return sizeof(aparse_list);
    if(arg->size == 0 && aparse__type_cmp(arg, APARSE_ARG_TYPE_STRING))
        return sizeof(char*);
    return arg->size;
}

static bool aparse__verify_layout(
        const aparse_arg *arg,
        int *invalid_idx)
{
    size_t prev_offset = 0, prev_size = 0;
    const size_t *layout = arg->data_layout;
    size_t layout_size = arg->layout_size;

    const aparse_arg *arg_ptr = arg->subargs;

    if (!arg->subargs && !layout)
        return false;

    if (!arg->subargs || !layout || layout_size <= 0)
    {
        *invalid_idx = -1;
        return false;
    }

    for(size_t i = 0; i < layout_size; i++)
    {
        size_t offset = 0, size = 0;
        if (!aparse_arg_nend(arg_ptr))
        {
            *invalid_idx = -1;
            return 0;
        }
        if (!aparse__is_argument(arg_ptr))
        {
            arg_ptr++;
            continue;
        }
        offset = layout[i * 2];
        size   = layout[i * 2 + 1];

        if (size == 0 ||
            offset < prev_offset + prev_size ||
            aparse__eval_size(arg_ptr) > size)
        {
            *invalid_idx = (int)i;
            return false;
        }

        prev_offset = offset;
        prev_size = size;
        arg_ptr++;
    }

    if(aparse_arg_nend(arg_ptr))
    {
        *invalid_idx = -1;
        return false;
    }

    return true;
}

static int aparse__fill_args_dest(
        const aparse_arg* arg, 
        uint8_t *buffer)
{
    size_t layout_size = 0;
    aparse_arg *arg_ptr = 0;
    if(!buffer)
        return APARSE_STATUS_OK;

    layout_size = arg->layout_size;
    if(layout_size == 0)
        return APARSE_STATUS_OK;

    arg_ptr = arg->subargs;
    for(size_t i = 0; i < layout_size && aparse_arg_nend(arg_ptr); 
            i++, arg_ptr++)
    {
        size_t offset = 0;
        if(!aparse__is_argument(arg_ptr))
            continue;
        offset  = arg->data_layout[i * 2];

        if(arg_ptr->ptr)
            continue;
        
        arg_ptr->ptr = &buffer[offset];
        if(arg_ptr->size == 0 && 
                aparse__type_cmp(arg_ptr, APARSE_ARG_TYPE_STRING))
            continue;
        arg_ptr->size = aparse__eval_size(arg_ptr);
    }
    return APARSE_STATUS_OK;
}

static void aparse__destroy_payload(
        const aparse_arg *args, 
        uint8_t *payload)
{
    if(!args || !payload)
        return;
    aparse__foreach(sa, args)
    {
        if(!(sa->type & APARSE_ARG_TYPE_ARRAY))
            continue;
        free(((aparse_list*)sa->ptr)->ptr);
    }
}

aparse_status aparse__check_missing(
        aparse_context* ctx,
        aparse_arg* args) 
{
    aparse_list missing_args = {.itemsz = sizeof(aparse_arg*)};
    aparse__tillend(item, args)
    {
        if(
                aparse__is_positional(item) &&
                !(item->flags & APARSE__ARG_PROCESSED) &&
                !(item->type & APARSE_ARG_TYPE_ARRAY && item->array_size == 0))
            aparse_list_add(&missing_args, &item);
    }
    if(missing_args.size > 0)
    {
        aparse__err_callback(ctx, APARSE_STATUS_MISSING_POSITIONAL,
                &missing_args, NULL, aparse__err_userdata);
        aparse_list_free(&missing_args);
        return APARSE_STATUS_MISSING_POSITIONAL;
    }

    return APARSE_STATUS_OK;
}


