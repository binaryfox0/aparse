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
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#    include <windows.h>
#    include <io.h>
#    include <sdkddkver.h>
#else
#    include <unistd.h>
#    include <sys/ioctl.h>
#endif

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
        ssize_t len = readlink("/proc/self/exe", 
                path, sizeof(path) - 1);
        const char *name = NULL;

        if(len <= 0 || (size_t)len >= sizeof(path) - 1)
            return;
        path[len] = '\0';
        name = strrchr(path, '/');
        snprintf(out_buf, size, "%s", name ? name + 1 : path);
    }
#endif
}

/* ========================================================================= */
/* MARK: Output-specific argument helpers                                    */
/* ========================================================================= */

/* Name of an option/flag as shown to the user in diagnostics. */
static APARSE__INLINE const char *aparse__arg_display_name(
        const aparse_argument_t *arg)
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

/* Flags and options, as opposed to positionals and subcommands. */
static APARSE__INLINE bool aparse__arg_is_optional(
        const aparse_argument_t *arg)
{
    return arg && (arg->kind == APARSE_ARGUMENT_FLAG || 
            arg->kind == APARSE_ARGUMENT_OPTION);
}

/* Find the subcommand of a SUBPARSERS argument whose scope is `scope_args`. */
static const aparse_argument_t *aparse__find_subcommand_by_scope(
        const aparse_argument_t *subparsers,
        const aparse_argument_t *scope_args)
{
    if(!subparsers || subparsers->kind != APARSE_ARGUMENT_SUBPARSERS || 
            !subparsers->children)
        return NULL;

    for(size_t i = 0; i < subparsers->children_count; i++)
        if(subparsers->children[i].children == scope_args)
            return &subparsers->children[i];

    return NULL;
}

/* ========================================================================= */
/* MARK: Output primitives                                                   */
/* ========================================================================= */

/*
 * The primitives below take a FILE *. When it is NULL nothing is written and
 * only the visible width is returned, so one code path both prints and
 * measures. Escape sequences never count towards the width.
 */
static int aparse__puts(
        FILE *file, 
        const char *str)
{
    if(!str)
        return 0;
    if(file)
        fputs(str, file);
    return (int)strlen(str);
}

static void aparse__set_color(
        FILE *file, 
        const char *color)
{
    (void)color;
    if(file)
        fputs(APARSE__ANSI_ESCAPE(color), file);
}

/* Print `str` wrapped in `color` followed by a reset. */
static int aparse__put_colored(
        FILE *file, 
        const char *color, 
        const char *str)
{
    int len = 0;

    aparse__set_color(file, color);
    len = aparse__puts(file, str);
    aparse__set_color(file, APARSE__COLOR_DEFAULT);
    return len;
}

static int aparse__pad(
        FILE *file, const int count)
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
            if(errno == 0 && end != env && *end == '\0' && 
                    width > 0 && width <= INT_MAX)
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

static void aparse__print_wrapped(
        const char *text, 
        const int start_col, 
        const int term_width)
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
        fwrite(word_start, 1, 
                (size_t)word_len, APARSE__STREAM);
        col += word_len;
    }
}

/* ========================================================================= */
/* MARK: Output type and default formatting                                  */
/* ========================================================================= */

static APARSE__INLINE const char *aparse__infer_metavar(
        const aparse_argument_t *arg)
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

static APARSE__INLINE const char *aparse__arg_type_string(
        const aparse_argument_t *arg)
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

#define APARSE__PRINT_DEFAULT_AS(T, FMT) \
    do { \
        T value; \
        memcpy(&value, arg->default_val, sizeof(value)); \
        fprintf(APARSE__STREAM, FMT, value); \
    } while(0)

static void aparse__print_default(
        const aparse_argument_t *arg)
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

static APARSE__INLINE bool aparse__has_printable_default(
        const aparse_argument_t *arg)
{
    return arg && arg->default_val &&
           (arg->kind == APARSE_ARGUMENT_POSITIONAL || 
            arg->kind == APARSE_ARGUMENT_OPTION) &&
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
            len += aparse__puts(file, arg->metavar ? 
                    arg->metavar : aparse__infer_metavar(arg));
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
                len += aparse__puts(file, 
                        APARSE__NULL_TO_EMPTY(arg->children[i].longopt));
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
static int aparse__print_command_path(
        const aparse_result_t *result)
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
                        APARSE__NULL_TO_EMPTY(
                            aparse__positional_name(&args[i])));
                continue;
            }

            subcmd = aparse__find_subcommand_by_scope(
                    &args[i], result->stack[idx + 1].args);
        }
        if(!subcmd)
            break;

        len += aparse__puts(APARSE__STREAM, " ");
        len += aparse__put_colored(APARSE__STREAM, 
                APARSE__COLOR_BOLD_MAGENTA,
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
        aparse__print_usage_part(APARSE__STREAM, 
                term_width, indent, arg, current_col);
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
    current_col += aparse__put_colored(APARSE__STREAM, 
            APARSE__COLOR_BOLD_BLUE, "usage: ");
    current_col += aparse__put_colored(APARSE__STREAM, 
            APARSE__COLOR_BOLD_MAGENTA, program_name);
    current_col += aparse__puts(APARSE__STREAM, " ");
    current_col += aparse__print_command_path(result);

    /* Arguments fold back to the column after the program and command path. */
    indent = current_col + 1;
    aparse__print_usage_group(result, true, 
            term_width, indent, &current_col);
    aparse__print_usage_group(result, false, 
            term_width, indent, &current_col);
    fputc('\n', APARSE__STREAM);
}

/* ========================================================================= */
/* MARK: Error output                                                        */
/* ========================================================================= */

static void aparse__print_string_list(
        const char *const *strings, 
        const size_t count)
{
    for(size_t i = 0; i < count; i++)
    {
        if(i > 0)
            fputs(", ", APARSE__STREAM);
        fputs(APARSE__NULL_TO_EMPTY(strings[i]), APARSE__STREAM);
    }
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
            APARSE__ERROR(program_name, 
                    "option '%s' expected %zu value%s.",
                    aparse__arg_display_name(result->arg), 
                    result->expected_count,
                    result->expected_count == 1 ? "" : "s");
            break;

        case APARSE_UERR_INVALID_VALUE:
            APARSE__ERROR(program_name, 
                    "invalid %s '%s'",
                    aparse__arg_type_string(result->arg), 
                    APARSE__NULL_TO_EMPTY(result->value));
            break;

        case APARSE_UERR_VALUE_OVERFLOW:
            APARSE__ERROR(program_name, 
                    "value '%s' is too large for argument '%s'",
                    APARSE__NULL_TO_EMPTY(result->value), 
                    aparse__arg_display_name(result->arg));
            break;

        case APARSE_UERR_VALUE_UNDERFLOW:
            APARSE__ERROR(program_name, 
                    "value '%s' underflows argument '%s'",
                    APARSE__NULL_TO_EMPTY(result->value), 
                    aparse__arg_display_name(result->arg));
            break;

        case APARSE_UERR_VERIFICATION:
            APARSE__ERROR(program_name, 
                    "invalid value '%s' for '%s': %s",
                    APARSE__NULL_TO_EMPTY(result->value), 
                    aparse__arg_display_name(result->arg),
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
                fputs(APARSE__NULL_TO_EMPTY(children[i].longopt), 
                        APARSE__STREAM);
            }
            fputs(")\n", APARSE__STREAM);
            break;
        }

        case APARSE_UERR_UNEXPECTED_ARGUMENT:
            aparse__log_prefix(program_name, APARSE__ERROR_LABEL);
            fputs("unrecognized argument: ", APARSE__STREAM);
            aparse__print_string_list(
                    result->strings, result->strings_count);
            fputc('\n', APARSE__STREAM);
            break;

        case APARSE_UERR_MISSING_POSITIONAL:
            aparse__log_prefix(program_name, APARSE__ERROR_LABEL);
            fputs("the following arguments are required: ", APARSE__STREAM);
            aparse__print_string_list(
                    result->strings, result->strings_count);
            fputc('\n', APARSE__STREAM);
            break;

        default:
            APARSE__ERROR(program_name, "%s", 
                    aparse_user_strerror(result->user_error));
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
        len += aparse__put_colored(file, 
                APARSE__COLOR_BOLD_GREEN, APARSE__NULL_TO_EMPTY(name));

        if(arg->shortopt && arg->longopt)
        {
            len += aparse__puts(file, ", ");
            len += aparse__put_colored(file, 
                    APARSE__COLOR_BOLD_CYAN, arg->longopt);
        }
        if(arg->kind == APARSE_ARGUMENT_OPTION)
        {
            len += aparse__puts(file, " ");
            len += aparse__put_colored(file, APARSE__COLOR_BOLD_YELLOW,
                    arg->metavar ? arg->metavar : aparse__infer_metavar(arg));
        }
    }
    else
        len += aparse__put_colored(file, 
                APARSE__COLOR_BOLD_GREEN, aparse__positional_name(arg));

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

    aparse__print_wrapped(APARSE__NULL_TO_EMPTY(arg->help), 
            help_position + 1, term_width);
    if(aparse__has_printable_default(arg))
    {
        fputs(" (default: ", APARSE__STREAM);
        aparse__print_default(arg);
        fputc(')', APARSE__STREAM);
    }
    fputc('\n', APARSE__STREAM);
}

/* Width of the label column: the widest label, capped at the help position. */
static int aparse__help_label_width(
        const aparse_argument_t *args, 
        const size_t args_count)
{
    int width = 0;

    for(size_t i = 0; i < args_count; i++)
    {
        width = APARSE__MAX(width, aparse__print_argument_label(
                    NULL, 1, &args[i]));
        if(args[i].kind != APARSE_ARGUMENT_SUBPARSERS || !args[i].children)
            continue;

        for(size_t j = 0; j < args[i].children_count; j++)
            width = APARSE__MAX(width, aparse__print_argument_label(
                        NULL, 2, &args[i].children[j]));
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
            aparse__put_colored(APARSE__STREAM, 
                    APARSE__COLOR_BOLD_BLUE, "positional arguments:");
            fputc('\n', APARSE__STREAM);
            printed = true;
        }

        label_len = aparse__print_argument_label(
                APARSE__STREAM, 1, &args[i]);
        aparse__print_help_text(label_len, label_width, 
                &args[i], term_width);
        if(args[i].kind != APARSE_ARGUMENT_SUBPARSERS || !args[i].children)
            continue;

        for(size_t j = 0; j < args[i].children_count; j++)
        {
            label_len = aparse__print_argument_label(
                    APARSE__STREAM, 2, &args[i].children[j]);
            aparse__print_help_text(label_len, label_width, 
                    &args[i].children[j], term_width);
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
            aparse__put_colored(APARSE__STREAM, 
                    APARSE__COLOR_BOLD_BLUE, "options:");
            fputc('\n', APARSE__STREAM);
            printed = true;
        }

        label_len = aparse__print_argument_label(
                APARSE__STREAM, 1, &args[i]);
        aparse__print_help_text(label_len, label_width, 
                &args[i], term_width);
    }
}

void aparse_print_help(const aparse_result_t *result)
{
    int term_width = 0;
    int label_width = 0;

    if(!result || !result->arg)
        return;

    term_width = aparse__get_terminal_width();
    label_width = aparse__help_label_width(
            result->arg, result->arg_count);
    aparse__print_help_positionals(
            result->arg, result->arg_count, 
            label_width, term_width);
    aparse__print_help_options(
            result->arg, result->arg_count, 
            label_width, term_width);
}
