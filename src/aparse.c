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

#include <stdlib.h>
#include <string.h>

static void *aparse__default_malloc(void *userdata, size_t size)
{
    const aparse_alloc_t *alloc = (const aparse_alloc_t *)userdata;

    if(!alloc)
        return malloc(size);

    return alloc->malloc ? alloc->malloc(alloc->userdata, size) :
        (alloc->realloc ? alloc->realloc(alloc->userdata, NULL, size) : 
         malloc(size));
}

static void *aparse__default_realloc(void *userdata, void *ptr, size_t size)
{
    const aparse_alloc_t *alloc = (const aparse_alloc_t *)userdata;

    if(!alloc)
        return realloc(ptr, size);

    return alloc->realloc ? alloc->realloc(alloc->userdata, ptr, size) : 
        realloc(ptr, size);
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
