#include "aparse_internal.h"

void aparse__default_errcb(
        const aparse_context* ctx,
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
            fprintf(stderr, "%s: " APARSE__ERROR_LABEL 
                    ": unrecognized arguments: ", aparse_progname);
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
            const int expected_count = *(const int*)field2;
            aparse_prog_error("option '%s' expected %d argument.",
                arg->flags & APARSE__ARG_SHORT_MATCH ? arg->shortopt : arg->longopt,
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
            aparse__lib__error("value '%s' is too large for argument '%s'",
                                     cargv, arg->longopt ? arg->longopt : arg->shortopt);
            break;
        }
        case APARSE_STATUS_UNDERFLOW:
        {
            const aparse_arg* arg = field1;
            const char* cargv = field2;
            if(arg->type & APARSE_ARG_TYPE_FLOAT)
            {
                aparse__lib__warn(
                        "value '%s' underflows precision argument '%s'",
                        cargv, arg->longopt ? arg->longopt : arg->shortopt);
            } else {
                aparse__lib__error(
                        "value '%s' underflows argument '%s'",
                        cargv, arg->longopt ? arg->longopt : arg->shortopt);
            }
            break;
        }
        case APARSE_STATUS_MISSING_POSITIONAL:
        {
            const aparse_list* args = field1;
            aparse__print_usage(ctx);
            fprintf(stderr, "%s: " APARSE__ERROR_LABEL 
                    ": the following arguments are required: ", aparse_progname);
            int printed = 0;
            for (size_t i = 0; i < args->size; i++) 
            {
                aparse_arg *item = aparse_list_get(args, aparse_arg*, i);
                if (printed++ > 0) 
                    fprintf(stderr, ", ");
                fprintf(stderr, "%s", item->longopt);
            }
            fprintf(stderr, "\n");
            break;
        }
        case APARSE_STATUS_INVALID_SUBCOMMAND:
        {
            const aparse_list* args = field1;
            const char* cargv = field2;
            aparse__print_usage(ctx);
            fprintf(stderr, "%s: " APARSE__ERROR_LABEL 
                    ": invalid choice: '%s' (choose from ", aparse_progname, cargv);
            aparse_arg* a = args->ptr;
            for(size_t i = 0; i < args->size; i++, a++)
            {
                fprintf(stderr, "%s%s", a->longopt, 
                        i < (args->size - 1) ? ", " : "");
            }

            fprintf(stderr, ")\n");
            break;
        }
        case APARSE_STATUS_NULL_POINTER:
        {
            const aparse_arg* arg = field1;
            aparse__lib__warn(
                    "non-null pointers was expected of argument '%s', skipped.",
                    arg->longopt ? arg->longopt : arg->shortopt);
            break;
        }
        case APARSE_STATUS_INVALID_TYPE:
        {
            const aparse_arg* arg = field1;
            aparse__lib__error(
                    "invalid argument type: 0x%04X of argument '%s'",
                    arg->type, arg->longopt ? arg->longopt : arg->shortopt);
            break;
        }
        case APARSE_STATUS_INVALID_SIZE:
        {
            const aparse_arg* arg = field1;
            const int size = *(const int*)field2;
            aparse__lib__error(
                    "invalid argument size: %d bytes of argument '%s'",
                    size, arg->longopt ? arg->longopt : arg->shortopt);
            break;
        }
        case APARSE_STATUS_INVALID_LAYOUT:
        {
            const aparse_arg* arg = field1;
            const int index = *(const int*)field2;
            aparse__lib__error("layout of \"%s\" at index %d was invalid",
                    arg->longopt, index);
            break;
        }
        case APARSE_STATUS_ALLOC_FAILURE:
        {
            aparse__lib__error(
                    "failed to allocate memory for argument parsing, "
                    "please retry again later.");
            break;
        }
        case APARSE_STATUS_UNHANDLED:
        {
            const aparse_arg* arg = field1;
            aparse__lib__error(
                    "unhandled %s size: %zu bytes of argument: '%s'",
                    arg->type & APARSE_ARG_TYPE_FLOAT ? "float" : "integer",
                    arg->size, arg->longopt ? arg->longopt : arg->shortopt
            );
            break;
        }

        case APARSE_STATUS_TOO_DEEP:
        {
            aparse__lib__error("parser nesting depth exceeded the limit");
            break;
        }
        default:
        {
            aparse__lib__error("unhandled error message");
            break;
        }
    }
}

void aparse__print_help(
        aparse_arg* main_args,
        aparse__context_t *ctx) 
{
    aparse__print_usage(ctx);
    printf("\n");
    if(ctx->stack_top == 1) 
    {
        if(aparse__desc)
            printf("%s\n\n", aparse__desc);
    }
    printf("positional arguments:\n");
    aparse__print_pos_help(main_args);
    printf("\noptions:\n");
    aparse__print_help_tag(&aparse__help_arg, APARSE__SPACE_PER_INDENT);

    for(aparse_arg *sa = main_args;
            aparse_arg_nend(sa); sa++)
    {
        if (
                aparse__is_argument(sa) && 
                !aparse__is_positional(sa))
            aparse__print_help_tag(sa, APARSE__SPACE_PER_INDENT);
    }
}

void aparse__print_wrapped(
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

        fwrite(word_start, 1, (size_t)word_len, stdout);
        col += word_len;
    }
}

void aparse__print_help_tag(
        const aparse_arg* arg,
        const int indent)
{
    int len = 0;
    if (!aparse__is_argument(arg) && !arg->help && !aparse_arg_nend(arg))
        return;

    len += printf("%*s", indent, "");
    if (arg->shortopt)
        len += printf("%s", arg->shortopt);
    if (arg->shortopt && arg->longopt)
        len += printf(", ");
    if (arg->longopt)
        len += printf("%s", arg->longopt);

    if (!aparse__is_positional(arg) &&
        !aparse__type_cmp(arg, APARSE_ARG_TYPE_BOOL))
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
        int space = APARSE__SPACE_PER_INDENT + MAX_ARG_STR -
            (longer ? 0 : len);

        if (longer)
            printf("\n");

        aparse__print_wrapped(
                arg->help, 
                space, 
                aparse__get_term_width());
    }

    printf("\n");
}

void aparse__print_pos_help(aparse_arg* args) 
{
    for (; aparse_arg_nend(args); args++) {
        if (aparse__is_positional(args)) {
            if(aparse__is_argument(args)) {
                aparse__print_help_tag(args, APARSE__SPACE_PER_INDENT);
                continue;
            }
            printf("%*s", APARSE__SPACE_PER_INDENT, "");
            aparse__print_subcmds(args);
            printf("\n");

            aparse__foreach(ptr, args)
            {
                aparse__print_help_tag(ptr, 
                        APARSE__SPACE_PER_INDENT * 2);
            }
        }
    }
}

void aparse__print_subcmds(
        const aparse_arg* args) 
{
    int index = 0;
    if (!args || !args->subargs)
    {
        fputs("{}", stderr);
        return;
    }

    fputc('{', stderr);
    aparse__foreach(ptr, args)
    {
        const char* name = ptr->longopt ? ptr->longopt : "";
        if (index++ > 0)
            fputs(", ", stderr);
        fputs(name, stderr);
    }

    fputc('}', stderr);
}

static void aparse__print_usage_before(
        const aparse_context *ctx) 
{
    int idx = 0;
    aparse_arg *arg = NULL;

    for(;;)
    {
        bool found = false;
        if(idx >= ctx->stack_top - 1)
            break;

        arg = ctx->stack[idx];
        for(; aparse_arg_nend(arg); arg++)
        {
            if(!aparse__is_positional(arg))
                continue;

            if(aparse__is_argument(arg))
                fprintf(stderr, "%s ", arg->longopt);
            else
            {
                aparse__foreach(subcmd, arg)
                {
                    if(subcmd->subargs == ctx->stack[idx + 1])
                    {
                        idx++;
                        found = true;
                        fprintf(stderr, "%s ", subcmd->longopt);
                        break;
                    }
                }

                if(found)
                    break;
            }
        }

        if(!found)
            break;
    }
}

static void aparse_print_usage_after(aparse_arg* args)
{
    aparse_list list = {.itemsz = sizeof(aparse_arg*)};
    for(aparse_arg *sa = args;
            aparse_arg_nend(sa); sa++)
    {
        if(aparse__is_positional(sa))
            aparse_list_add(&list, &sa);
        else 
        {
            fprintf(stderr, "[%s", 
                    sa->shortopt ? sa->shortopt : sa->longopt);
            if(!aparse__type_cmp(sa, APARSE_ARG_TYPE_BOOL))
            {
                const char *option = 0;
                size_t idx = 0;
                
                option = sa->longopt ? sa->longopt : sa->shortopt;
                idx = aparse__option_value_index(option);
                fputc(' ', stderr);
                for(const char *ch = option + idx; *ch != '\0'; ch++)
                    fputc(toupper(*ch), stderr);
            }
            fputs("] ", stderr);
        }
    }
    for(size_t i = 0; i < list.size; i++)
    {
        aparse_arg* entry = aparse_list_get(&list, aparse_arg*, i);
        if(aparse__is_argument(entry))
            fprintf(stderr, "%s ", entry->longopt);
        else {
            aparse__print_subcmds(entry);
            fputs(" ... ", stderr);
        }
    }
    aparse_list_free(&list);
    fputc('\n', stderr);
}

void aparse__print_usage(
        const aparse_context *ctx) 
{
    fprintf(stderr, "usage: %s ", aparse_progname);
    aparse__print_usage_before(ctx);
    aparse_print_usage_after(ctx->stack[ctx->stack_top - 1]);
}

void aparse__reset_state(
        aparse_arg *args)
{
    if(!args)
        return;
    aparse__tillend(arg, args)
    {
        if(
                aparse__is_positional(arg) && 
                !aparse__is_argument(arg))
        {
            aparse__foreach(subcmd, arg)
                aparse__reset_state(subcmd->subargs);
        }

        arg->flags = 0;
    }
}

aparse_arg* aparse__argv_match(
        const char* argv, 
        aparse_arg* args)
{
    aparse_arg* positional = NULL;
    if(
            !strcmp(argv, aparse__help_arg.shortopt) || 
            !strcmp(argv, aparse__help_arg.longopt))
        return (aparse_arg*)(uintptr_t)&aparse__help_arg;
    
    for(aparse_arg *sa = args;
            aparse_arg_nend(sa); sa++)
    {
        if (aparse__is_positional(sa)) {
            if (!(sa->flags & APARSE__ARG_PROCESSED))
                if(!positional)
                    positional = sa;
            continue;
        }
        if (sa->shortopt) 
        {
            size_t shortlen = strlen(sa->shortopt);
            if (strncmp(argv, sa->shortopt, shortlen) == 0) {
                sa->flags |= APARSE__ARG_SHORT_MATCH;
                if (argv[shortlen] == '\0') {
                    return sa;
                } else if (argv[shortlen] == '=') {
                    sa->flags |= APARSE__ARG_EQUAL_VAL;
                    return sa;
                }
            }
        }
        if (sa->longopt) 
        {
            size_t longlen = strlen(sa->longopt);
            if (strncmp(argv, sa->longopt, longlen) == 0) {
                sa->flags &= (uint8_t)~APARSE__ARG_SHORT_MATCH;
                if (argv[longlen] == '\0') {
                    return sa;
                } else if (argv[longlen] == '=') {
                    sa->flags |= APARSE__ARG_EQUAL_VAL;
                    return sa;
                }
            }
        }
    }
    return positional;
}

const char* aparse__get_exename(
        const char* argv0)
{
    const char* last_slash = strrchr(argv0, '/');
#ifdef _WIN32
    const char* last_backslash = strrchr(argv0, '\\');
    if (!last_slash || (last_backslash && last_backslash > last_slash))
        last_slash = last_backslash;
#endif
    return last_slash ? last_slash + 1 : argv0;
}

size_t aparse__option_value_index(
        const char* longopt)
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

int aparse__get_term_width(void)
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

