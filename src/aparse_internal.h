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

#ifndef APARSE_INTERNAL_H
#define APARSE_INTERNAL_H

#include <aparse/aparse.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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

/* ========================================================================= */
/* MARK: Shared internal functions                                           */
/* ========================================================================= */

bool aparse__config_is_valid(const aparse_config_t *config);
const char *aparse__positional_name(const aparse_argument_t *arg);
bool aparse__arg_is_array(const aparse_argument_t *arg);
void aparse__log_prefix(const char *source, const char *type);
APARSE__PRINTF(3, 4) void aparse__log(
        const char *source,
        const char *type,
        APARSE__PRINTF_FMT const char *fmt,
        ...);

#define APARSE__DEBUG_LABEL APARSE__COLORIZE(APARSE__COLOR_BOLD_CYAN, "debug")
#define APARSE__INFO_LABEL  APARSE__COLORIZE(APARSE__COLOR_BOLD_BLUE, "info")
#define APARSE__WARN_LABEL  APARSE__COLORIZE(APARSE__COLOR_BOLD_YELLOW, "warn")
#define APARSE__ERROR_LABEL APARSE__COLORIZE(APARSE__COLOR_BOLD_RED, "error")
#define APARSE__DEBUG(source, ...) aparse__log(source, APARSE__DEBUG_LABEL, __VA_ARGS__)
#define APARSE__INFO(source, ...)  aparse__log(source, APARSE__INFO_LABEL, __VA_ARGS__)
#define APARSE__WARN(source, ...)  aparse__log(source, APARSE__WARN_LABEL, __VA_ARGS__)
#define APARSE__ERROR(source, ...) aparse__log(source, APARSE__ERROR_LABEL, __VA_ARGS__)

#endif /* APARSE_INTERNAL_H */
