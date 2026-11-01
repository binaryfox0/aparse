#include "aparse_internal.h"

#include <stdarg.h>

const char* aparse_progname = 0;
const char *aparse__desc = 0;
aparse_error_callback aparse__err_callback = 0;
void* aparse__err_userdata = 0;

aparse_status aparse_parse(
        const int argc,
        char* const * argv,
        aparse_arg* args,
        aparse_list* out_dispatch,
        const char* program_desc)
{
    aparse_status ret = APARSE_STATUS_OK;
    aparse__context_t ctx = {0};
    aparse_list unknown_list = {0};
    aparse_list dispatch_list = {0};

    if(!argv || argc < 1)
        return APARSE_STATUS_FAILURE;
    aparse_progname = aparse__get_exename(argv[0]);
    aparse__desc = program_desc;

    if(!args)
        return APARSE_STATUS_OK;

    unknown_list.itemsz = sizeof(const char*);
    dispatch_list.itemsz = sizeof(aparse__dispatch_t);

    ctx.idx = 1;
    ctx.unknown = &unknown_list;
    ctx.dispatch = &dispatch_list;

    if(!aparse__err_callback)
        aparse_set_error_callback(NULL, NULL);

    aparse__reset_state(args);
    ret = aparse__parse_impl(argc, argv, args, &ctx);
    if(ret == APARSE_STATUS_OK)
        ret = aparse__check_missing(&ctx, args);

    if(ret == APARSE_STATUS_OK && unknown_list.size > 0)
    {
        aparse__raise_nonfatal(&ctx, APARSE_STATUS_UNKNOWN_ARGUMENT,
                &unknown_list, NULL);
        ret = APARSE_STATUS_FAILURE;
    }
    aparse_list_free(&unknown_list);

    if(ret == APARSE_STATUS_OK)
    {
        if(out_dispatch)
            *out_dispatch = dispatch_list;
        else
            aparse_dispatch_all(&dispatch_list);
    } else {
        aparse_dispatch_free(&dispatch_list);
    }

    return ret;
}

void aparse_log(
        const char *source,
        const char *type,
        const char *fmt,
        ...)
{
#ifdef APARSE_LOG_STDOUT
    FILE *stream = stdout;
#else
    FILE *stream = stderr;
#endif

    if(source)
    {
        fputs(source, stream);
        fputs(": ", stream);
    }

    if(type)
    {
        fputs(type, stream);
        fputs(": ", stream);
    }

    if(fmt)
    {
        va_list va;
        va_start(va, fmt);
        vfprintf(stream, fmt, va);
        va_end(va);
        fputc('\n', stream);
    }
}

void aparse_set_error_callback(const aparse_error_callback cb, void* userdata)
{
    if(!cb)
    {
        aparse__err_callback = aparse__default_errcb;
        aparse__err_userdata = NULL;
        return;
    }

    aparse__err_callback = cb;
    aparse__err_userdata = userdata;
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
        [APARSE_STATUS_UNHANDLED]           = "Unhandled type of argument.",
        [APARSE_STATUS_TOO_DEEP]            = "Parser nesting depth exceeded the limit.",
    };

    if(status < 0 || status >= __APARSE_STATUS_ENUM_END__)
        return "Unknown error";
    return error_msg[status];
}
