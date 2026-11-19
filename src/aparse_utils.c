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

#include <stdarg.h>
#include <stdio.h>

/* ========================================================================= */
/* MARK: Logging                                                             */
/* ========================================================================= */

/* Print the "<source>: <type>: " prefix shared by all diagnostics. */
void aparse__log_prefix(
        const char *source, 
        const char *type)
{
    fputs(source ? source : "aparse", APARSE__STREAM);
    fputs(": ", APARSE__STREAM);
    fputs(type ? type : "", APARSE__STREAM);
    fputs(": ", APARSE__STREAM);
}

APARSE__PRINTF(3, 4) void aparse__log(
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
/* MARK: Configuration helpers                                              */
/* ========================================================================= */

bool aparse__config_is_valid(
        const aparse_config_t *config)
{
    return config && config->alloc.realloc && config->alloc.free;
}

/* ========================================================================= */
/* MARK: Argument helpers                                                    */
/* ========================================================================= */

/* Name of a positional/command as shown to the user: metavar wins. */
const char *aparse__positional_name(
        const aparse_argument_t *arg)
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

bool aparse__arg_is_array(const aparse_argument_t *arg)
{
    return arg && arg->kind == APARSE_ARGUMENT_OPTION &&
           (arg->action == APARSE_ACTION_APPEND || 
            APARSE__NARGS_IS_FLEXIBLE(APARSE_NARGS_MODE(arg->nargs)));
}
