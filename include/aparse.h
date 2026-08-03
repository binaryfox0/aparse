/*
MIT License

Copyright (c) 2025 binaryfox0

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef APARSE_H
#define APARSE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <aparse_list.h>

/** @cond HIDDEN */

#if defined(_MSC_VER)
#   define APARSE__INLINE __forceinline
#else
#   define APARSE__INLINE static inline __attribute__((always_inline))
#endif

#if !defined(APARSE_STDC_COMPLIANT) || \
    (defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL == 0) \
        || defined(__GNUC__) || defined(__clang__)

#   define APARSE__CAT_IMPL(a, b) a##b
#   define APARSE__CAT(a, b) APARSE__CAT_IMPL(a, b)

#   define APARSE__COUNT_ARGS2( \
        _0, _1, _2, _3, _4, _5, _6, _7, _8, \
        _9, _10,_11,_12,_13,_14,_15,_16,N,...) N
#   define APARSE__COUNT_ARGS1(...) \
        APARSE__COUNT_ARGS2(dummy, ##__VA_ARGS__, \
            16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
#   define APARSE__COUNT_ARGS(...) \
        APARSE__COUNT_ARGS1(__VA_ARGS__)

#   define APARSE__MAP_0(...)           0
#   define APARSE__MAP_1(m, a, x)       m(a, x)
#   define APARSE__MAP_2(m, a, x, ...)  m(a, x), APARSE__MAP_1(m, a, __VA_ARGS__)
#   define APARSE__MAP_3(m, a, x, ...)  m(a, x), APARSE__MAP_2(m, a, __VA_ARGS__)
#   define APARSE__MAP_4(m, a, x, ...)  m(a, x), APARSE__MAP_3(m, a, __VA_ARGS__)
#   define APARSE__MAP_5(m, a, x, ...)  m(a, x), APARSE__MAP_4(m, a, __VA_ARGS__)
#   define APARSE__MAP_6(m, a, x, ...)  m(a, x), APARSE__MAP_5(m, a, __VA_ARGS__)
#   define APARSE__MAP_7(m, a, x, ...)  m(a, x), APARSE__MAP_6(m, a, __VA_ARGS__)
#   define APARSE__MAP_8(m, a, x, ...)  m(a, x), APARSE__MAP_7(m, a, __VA_ARGS__)
#   define APARSE__MAP_9(m, a, x, ...)  m(a, x), APARSE__MAP_8(m, a, __VA_ARGS__)
#   define APARSE__MAP_10(m, a, x, ...) m(a, x), APARSE__MAP_9(m, a, __VA_ARGS__)
#   define APARSE__MAP_11(m, a, x, ...) m(a, x), APARSE__MAP_10(m, a, __VA_ARGS__)
#   define APARSE__MAP_12(m, a, x, ...) m(a, x), APARSE__MAP_11(m, a, __VA_ARGS__)
#   define APARSE__MAP_13(m, a, x, ...) m(a, x), APARSE__MAP_12(m, a, __VA_ARGS__)
#   define APARSE__MAP_14(m, a, x, ...) m(a, x), APARSE__MAP_13(m, a, __VA_ARGS__)
#   define APARSE__MAP_15(m, a, x, ...) m(a, x), APARSE__MAP_14(m, a, __VA_ARGS__)
#   define APARSE__MAP_16(m, a, x, ...) m(a, x), APARSE__MAP_15(m, a, __VA_ARGS__)

#   define APARSE__OFFSETOF_AND_SIZEOF(s, m) offsetof(s, m), sizeof(((s*)0)->m )
#   define APARSE__OFFSETOFS(s, ...) { APARSE__EXPAND(APARSE__MAP(APARSE__OFFSETOF_AND_SIZEOF, s, __VA_ARGS__)) }

#   define APARSE__EXPAND(...) __VA_ARGS__
#   define APARSE__MAP(m, a, ...) \
    APARSE__CAT(APARSE__MAP_, APARSE__COUNT_ARGS(__VA_ARGS__))(m, a, __VA_ARGS__)

#   define APARSE__FIRST_ARG(a, ...) a
#   define APARSE__NOT_FIRST_ARG(a, ...) __VA_ARGS__


/** @endcond */

/**
 * @brief Create a subparser argument entry for @ref aparse_arg_s.
 *
 * This macro wraps `aparse_arg_subparser_impl` and automatically generates
 * the `data_layout` array for the specified members of `data_struct`.
 *
 * @param name        The argument name (string).
 * @param subargs     Pointer to subparser argument array.
 * @param handle      Function pointer to handler called after parsing.
 * @param buffer      Pointer to user-defined buffer described by the layout
 * @param size        The size of the buffer 
 * @param help        Help string describing this subparser.
 * @param data_struct Struct type containing members to map.
 * @param ...         List of member names of `data_struct` to include in the layout.
 *
 * @note Uses compiler-specific variadic macro expansions; some older compilers
 *       may not support it correctly
 *
 * @example
 * @code{.c}
 * struct config {
 *     int port;
 *     char *name;
 * };
 * aparse_arg_subparser(
 *          "config", 
 *          config_subargs, 
 *          handle_config, 
 *          NULL,
 *          0,
 *          "Config subparser", 
 *          config, port, name);
 * @endcode
 */
#   define aparse_arg_subparser( \
        name, \
        subargs, \
        handle, \
        buffer, \
        size, \
        help, \
        ...) \
       aparse_arg_subparser_impl( \
               (name), \
               (subargs), \
               (handle), \
               (buffer), \
               (size), \
               (help), \
               (size_t[]) \
                    APARSE__OFFSETOFS( \
                        APARSE__FIRST_ARG(__VA_ARGS__), \
                        APARSE__NOT_FIRST_ARG(__VA_ARGS__)), \
                APARSE__COUNT_ARGS(APARSE__NOT_FIRST_ARG(__VA_ARGS__)) \
        )
#endif

/** @} */ // end of aparse_macros

/** @cond HIDDEN */
#ifndef APARSE_NO_ANSI_ESCAPE
#    ifdef _WIN32
#       include <sdkddkver.h>
#       if defined(NTDDI_VERSION) && (NTDDI_VERSION >= NTDDI_WIN10_TH2)
#           define APARSE__ANSI_ESCAPE(x) x
#       else
#           define APARSE__ANSI_ESCAPE(x)
#       endif
#    else
#       define APARSE__ANSI_ESCAPE(x) x
#    endif
#endif

#define APARSE__DEBUG_LABEL \
    APARSE__ANSI_ESCAPE("\x1b[1;36m") "debug" APARSE__ANSI_ESCAPE("\x1b[0m")
#define APARSE__INFO_LABEL  \
    APARSE__ANSI_ESCAPE("\x1b[1;34m") "info"  APARSE__ANSI_ESCAPE("\x1b[0m")
#define APARSE__WARN_LABEL  \
    APARSE__ANSI_ESCAPE("\x1b[1;33m") "warn"  APARSE__ANSI_ESCAPE("\x1b[0m") 
#define APARSE__ERROR_LABEL \
    APARSE__ANSI_ESCAPE("\x1b[1;31m") "error" APARSE__ANSI_ESCAPE("\x1b[0m") 

#if defined(__GNUC__) || defined(__clang__)
#   define APARSE__PRINTF(fmt_index, arg_index) \
        __attribute__((format(printf, fmt_index, arg_index)))
#   define APARSE__PRINTF_FMT
#elif defined(_MSC_VER)
#   define APARSE__PRINTF(fmt_index, arg_index)
#   define APARSE__PRINTF_FMT _Printf_format_string_
#else
#   define APARSE__PRINTF(fmt_index, arg_index)
#   define APARSE__PRINTF_FMT
#endif
/** @endcond */

/**
 * @brief Print informational message to stderr, with color if supported
 */
#define aparse_prog_debug(...) \
    aparse_log(aparse_progname, APARSE__DEBUG_LABEL, __VA_ARGS__)

/**
 * @brief Print informational message to stderr, with color if supported
 */
#define aparse_prog_info(...) \
    aparse_log(aparse_progname, APARSE__DEBUG_LABEL, __VA_ARGS__)

/**
 * @brief Print warning message to stderr, with color if supported
 */
#define aparse_prog_warn(...) \
    aparse_log(aparse_progname, APARSE__DEBUG_LABEL, __VA_ARGS__)

/**
 * @brief Print error message to stderr, with color if supported
 */
#define aparse_prog_error(...) \
    aparse_log(aparse_progname, APARSE__DEBUG_LABEL, __VA_ARGS__)


#ifdef __cplusplus
extern "C" {
#endif

extern const char* aparse_progname;
APARSE__PRINTF(3, 4) void aparse_log(
        const char *source,
        const char *type,
        APARSE__PRINTF_FMT const char *fmt,
        ...);

/**
 * @enum aparse_arg_types
 * @brief Enumerates all possible argument types recognized by the parser.
 *
 * These values define how each @ref aparse_arg_s entry should be interpreted.
 * Some constants are combinations or modifiers of others (bitmask-style flags).
 *
 * @see aparse_arg_s
 */
typedef enum aparse_arg_types
{
    /**
     * @brief Sign flag mask for signed argument types.
     * Used internally to mark that an argument type is signed.
     */
    APARSE_ARG_TYPE_SIGNED_FLAGS = (1 << 7),

    /**
     * @brief Unknown or uninitialized argument type.
     */
    APARSE_ARG_TYPE_UNKNOWN = 0,

    /**
     * @brief String argument type.
     * Stores a pointer to a null-terminated string.
     */
    APARSE_ARG_TYPE_STRING,

    /**
     * @brief Boolean argument type.
     * Typically represents flags like `--verbose` or `--quiet`.
     */
    APARSE_ARG_TYPE_BOOL,

    /**
     * @brief Unsigned integer argument type.
     */
    APARSE_ARG_TYPE_UNSIGNED,

    /**
     * @brief Floating-point argument type.
     */
    APARSE_ARG_TYPE_FLOAT,

    /**
     * @brief Array argument type.
     * Indicates that the argument points to a list of values 
     * rather than a single one.
     */
    APARSE_ARG_TYPE_ARRAY = (1 << 3),

    /**
     * @brief Positional argument type.
     * Used for arguments that are identified by their position, 
     * not by an option name.
     */
    APARSE_ARG_TYPE_POSITIONAL = (1 << 4),

    /**
     * @brief Normal argument type.
     * Used for standard named options like `--file` or `-f`.
     */
    APARSE_ARG_TYPE_ARGUMENT   = (1 << 5),

    /**
     * @brief Subparser or subcommand argument type.
     * Equivalent to @ref APARSE_ARG_TYPE_POSITIONAL, 
     * but used for defining subcommands.
     */
    APARSE_ARG_TYPE_SUBPARSER  = APARSE_ARG_TYPE_POSITIONAL,

    /**
     * @brief Signed integer argument type.
     * Combines @ref APARSE_ARG_TYPE_UNSIGNED with @ref APARSE_ARG_TYPE_SIGNED_FLAGS.
     */
    APARSE_ARG_TYPE_SIGNED = APARSE_ARG_TYPE_UNSIGNED | APARSE_ARG_TYPE_SIGNED_FLAGS,

    /**
     * @brief Bitmask used to extract base argument type.
     * Can be applied to a type value to ignore modifier flags.
     */
    APARSE_ARG_TYPE_BITMASK = 0x7
} aparse_arg_types;

/**
 * @brief Describes a single argument, option, or subparser definition.
 */
typedef struct aparse_arg aparse_arg;

/**
 * @brief Handler function called after successful subparser parsing.
 * 
 * @param arg  Pointer to `aparse_arg` contain information about the caller
 * @param data Pointer to the data structure described by @ref data_layout.
 */
typedef void (*aparse_handler_t)(const aparse_arg *arg, void *data);

/**
 * @struct aparse_arg
 * @brief Describes a single argument, option, or subparser definition.
 *
 * The `aparse_arg` structure defines the metadata for each command-line
 * argument or subparser, including its option names, data type, storage
 * pointer, and parsing behavior.
 *
 * The meaning of certain members depends on the argument type
 * (see @ref aparse_arg_types).
 */
typedef struct aparse_arg
{
    /**
     * @brief The short option name (e.g., "-h") for optional arguments.
     *
     * - Ignored for positional arguments.  
     * - May be `NULL` if `longopt` is used instead.
     */
    const char* shortopt;

    /**
     * @brief The long option name (e.g., "--help") for optional arguments.
     *
     * - Required for positional arguments.  
     * - May be `NULL` if `shortopt` is present and sufficient.
     */
    const char* longopt;

    /**
     * @brief A help message describing the argument.
     *
     * The message will be displayed in help outputs generated by the parser.
     * Can be NULL
     */
    const char* help;

    /**
     * @brief The argument type.
     *
     * Indicates whether the argument is a string, integer, array, subparser, etc.
     * See @ref aparse_arg_types for all possible values.
     */
    aparse_arg_types type : 8;

    /**
     * @brief Internal parser flags to track parsing process.
     *
     * Typically not set manually by the user.
     */
    uint8_t flags;

    /**
     * @brief Type-dependent size or count parameter.
     *
     * The meaning of this field depends on the argument type:
     *
     * **1. Normal arguments (`APARSE_ARG_TYPE_ARGUMENT`)**
     * - Defines the size (in bytes) of the destination variable pointed to by `ptr`.
     * - For string arguments (`APARSE_ARG_TYPE_STRING`):
     *   - If `size == 0`, the parser assigns `argv[index]` directly to `*ptr`
     *     (do not free it). The user must pass `const char**` as the target pointer.
     * - Recommended: use the `sizeof()` operator on the destination variable.
     *
     * **2. Array arguments (`APARSE_ARG_TYPE_ARRAY`)**
     * - Defines the number of elements in the destination array.
     *
     * **3. Subparser arguments (`APARSE_ARG_TYPE_SUBPARSER`)**
     * - Defines the number of offset-size pairs in @ref data_layout.
     */
    size_t size;
             
    /**
     * @brief Pointer to the variable where the parsed value will be stored.
     */
    void* ptr;

    /**
     * @brief Type-specific data fields.
     *
     * The union contains either:
     * - Members used for normal/array arguments (`ptr`, `element_size`), or
     * - Members used for subparser/subcommand definitions (`subargs`, `handler`, `data_layout`).
     */
    union {
       /**
         * @brief Argument-only fields (used when `type` include `APARSE_ARG_TYPE_ARGUMENT`).
         */
        struct {
            /**
             * @brief Desired size of the array arguments
             */
            size_t array_size;
            /**
             * @brief Size of each element for array arguments.
             */
            size_t element_size;
        };
        // For subparsers/subcommands
        struct {
            /**
             * @brief Array of subarguments used in the subcommand.
             */
            struct aparse_arg* subargs;

            /**
             * @brief Handler function called after successful subparser parsing.
             */
            aparse_handler_t handler;

            /**
             * @brief Array describing the data layout passed to the handler.
             *
             * Each pair in the array represents `{offset, size}` for a member in the
             * destination structure. Example:
             *
             * @code{.c}
             * struct config {
             *     int port;
             *     char *name;
             * };
             *
             * int layout[] = {
             *     offsetof(struct config, port), sizeof(int),
             *     offsetof(struct config, name), sizeof(char*)
             * };
             * @endcode
             */
            const size_t *data_layout;

            /**
             * @brief The size of \p data_layout
             */
            size_t layout_size;
        };
    };
} aparse_arg;

/**
 * @enum aparse_status
 * @brief Status codes reported to error callbacks.
 */
typedef enum aparse_status
{
    APARSE_STATUS_OK = 0,               /**< Parsing succeeded with no errors. */
    
    APARSE_STATUS_FAILURE,              /**< General parsing failure (unspecified). */
    APARSE_STATUS_UNKNOWN_ARGUMENT,     /**< Unrecognized command-line argument or option. */

    APARSE_STATUS_MISSING_VALUE,        /**< Options/Arrays expected multiple value, but none was provided */
    APARSE_STATUS_INVALID_VALUE,        /**< Value provided for the argument was invalid */
    APARSE_STATUS_OVERFLOW,             /**< Numeric value exceeds supported range */
    APARSE_STATUS_UNDERFLOW,            /**< Numeric value is below supported range */

    APARSE_STATUS_MISSING_POSITIONAL,   /**< Expected positional argument was not provided. */
    APARSE_STATUS_INVALID_SUBCOMMAND,   /**< Subcommand not found or unrecognized. */

    APARSE_STATUS_NULL_POINTER,         /**< A required pointer argument was NULL. */
    APARSE_STATUS_INVALID_TYPE,         /**< Argument type is invalid or mismatched. */
    APARSE_STATUS_INVALID_SIZE,         /**< Argument size is invalid for its type. */

    APARSE_STATUS_INVALID_LAYOUT,       /**< The layout is invalid */

    APARSE_STATUS_ALLOC_FAILURE,        /**< Memory allocation failed. */
    APARSE_STATUS_UNHANDLED,            /**< Unhandled type of argument. */
    APARSE_STATUS_TOO_DEEP,             /**< Parser nesting depth exceeded the limit */

    __APARSE_STATUS_ENUM_END__          /**< The marker for the end of aparse_status. THIS MUST BE AT THE END */
} aparse_status;

/**
 * @brief Opaque parsing context.
 *
 * Contains the internal state of the parser and may be passed to
 * callbacks for accessing parser-related information.
 */
typedef struct aparse_context aparse_context;

/**
 * @brief Error callback function type for reporting parsing issues.
 *
 * @param status     The error code (see ::aparse_status).
 * @param field1     Context-specific first data pointer.
 * @param field2     Context-specific secondary pointer (may be NULL).
 * @param userdata   The user-provided pointer from the parser context.
*
 * @note
 * The parser never frees or modifies the data passed through @p field1 or @p field2.
 * The callback should treat them as read-only.
 *
 * | Status code                        | field1 type            | field2 type            | Description                                       |
 * |------------------------------------|------------------------|------------------------|---------------------------------------------------|
 * | ::APARSE_STATUS_UNKNOWN_ARGUMENT   | `unknown_args`         | `NULL`                 | Unknown argument name.                            |
 * | ::APARSE_STATUS_MISSING_VALUE      | `current_arg`          | `expected_count`       | Option definition that requires a value.          |
 * | ::APARSE_STATUS_INVALID_VALUE      | `current_arg`          | `current_argv`         | Argument definition and invalid value string.     |
 * | ::APARSE_STATUS_OVERFLOW           | `current_arg`          | `current_argv`         | Numeric argument and overflowing value string.    |
 * | ::APARSE_STATUS_UNDERFLOW          | `current_arg`          | `current_argv`         | Numeric argument and underflowing value string.   |
 * | ::APARSE_STATUS_MISSING_POSITIONAL | `required_args`        | `NULL`                 | Missing positional argument definition.           |
 * | ::APARSE_STATUS_INVALID_SUBCOMMAND | `parser_subargs`       | `current_argv`         | Invalid subcommand name.                          |
 * | ::APARSE_STATUS_NULL_POINTER       | `current_arg`          | `NULL`                 | Invalid NULL pointer in user argument definition. |
 * | ::APARSE_STATUS_INVALID_TYPE       | `current_arg`          | `NULL`                 | Type mismatch in argument definition.             |
 * | ::APARSE_STATUS_INVALID_SIZE       | `current_arg`          | `arg_size`             | Invalid size field in argument definition.        |
 * | ::APARSE_STATUS_INVALID_LAYOUT     | `current_arg`          | `index`                | The layout was invalid at index                   |
 * | ::APARSE_STATUS_ALLOC_FAILURE      | `NULL`                 | `NULL`                 | Memory allocation failed inside parser.           |
 * | ::APARSE_STATUS_UNHANDLED          | `current_arg`          | `NULL`                 | An unhandled type of argument.                    |
 * | ::APARSE_STATUS_TOO_DEEP           | `NULL`                 | `NULL`                 | Parser nesting depth exceeded the limit           |
 *
 * - `const aparse_list* unknown_args  `: An aparse_list refer to a list of arguments. `unknown_args.ptr` should be converted into `aparse_arg*`
 * - `const aparse_arg*  current_arg   `: An aparse_arg* refer to the currently processed argument.
 * - `const int*         expected_count`: The count of expected argv of `current_arg`
 * - `const char*        cargv         `: The current argv is currently being processed
 * - `const aparse_list* required_args `: An aparse_list refer to a list of required arguments. `required_args.ptr` should be converted into `aparse_arg*`
 * - `const int*         size          `: The invalid size of `current_arg`. It can be `current_arg.size` or `current_arg.element_size`
 * - `const int*         index         `: The base index of current entry inside `current_arg.data_layout`
 */
typedef void (*aparse_error_callback)(
        const aparse_context *ctx,
        const aparse_status status, 
        const void* field1, 
        const void* field2, 
        void *userdata);

/**
 * @brief Create an option argument (flag with value).
 *
 * @param shortopt   Short option string (e.g., "-o"), may be NULL.
 * @param longopt    Long option string (e.g., "--output"), may be NULL.
 * @param dest       Pointer to destination variable.
 * @param size       Size of destination variable in bytes.
 * @param type       Value type (see ::aparse_arg_types).
 * @param help       Help string (optional).
 *
 * @return Constructed ::aparse_arg definition.
 */
APARSE__INLINE aparse_arg aparse_arg_option(
        const char* shortopt, 
        const char* longopt, 
        void* dest, 
        const size_t size, 
        const aparse_arg_types type, 
        const char* help) 
{
    return (aparse_arg){
        .shortopt = shortopt, 
        .longopt = longopt,
        .type = type | 
            APARSE_ARG_TYPE_ARGUMENT, 
        .ptr = dest, 
        .size = size, 
        .help = help,
    };
}

/**
 * @brief Create a numeric positional argument.
 *
 * Defines a positional argument that expects a numeric value, such as an integer or float.
 * Typically used for command-line arguments that represent numbers, counts, or indices.
 *
 * @param name  Argument name (used for help and matching).
 * @param dest  Pointer to destination variable where the parsed number is stored.
 * @param size  Size of the destination variable in bytes.
 * @param type  Data type of the number (e.g., ::APARSE_ARG_TYPE_SIGNED, ::APARSE_ARG_TYPE_FLOAT).
 * @param help  Optional help string to describe this argument.
 *
 * @return A fully constructed ::aparse_arg definition for numeric positional arguments.
 */
APARSE__INLINE aparse_arg aparse_arg_number(
        const char* name, 
        void* dest, 
        const size_t size, 
        const aparse_arg_types type, 
        const char* help) 
{
    return (aparse_arg){
        .longopt = name, 
        .ptr = dest, 
        .size = size, 
        .help = help,
        .type = type | 
            APARSE_ARG_TYPE_POSITIONAL | 
            APARSE_ARG_TYPE_ARGUMENT
    };    
}

/**
 * @brief Create a string positional argument.
 *
 * Defines a positional argument that accepts a single string value.
 * Typically used for filenames, paths, or text parameters.
 *
 * @param name  Argument name (used for help and matching).
 * @param dest  Pointer to destination buffer where the string will be stored.
 * @param size  Size of the destination buffer in bytes.
 * @param help  Optional help string to describe this argument.
 *
 * @return A fully constructed ::aparse_arg definition for string positional arguments.
 *
 * @note If @p size is 0, `dest` will be assigned with the string of argument (`const char*`)
 */
APARSE__INLINE aparse_arg aparse_arg_string(
        const char* name, 
        void* dest, 
        const size_t size, 
        const char* help) 
{
    return (aparse_arg) {
        .longopt = name, 
        .ptr = dest, 
        .size = size,
        .help = help,
        .type = APARSE_ARG_TYPE_STRING |
            APARSE_ARG_TYPE_POSITIONAL |
            APARSE_ARG_TYPE_ARGUMENT
    };
}

/**
 * @brief Create a subparser definition.
 *
 * Defines a subparser (subcommand) entry with its own argument list and handler function.
 * Useful for CLI tools with multiple modes or subcommands, e.g.:
 * @code
 * mytool build ...
 * mytool test ...
 * @endcode
 *
 * @param name           Subcommand name (e.g. "build", "test").
 * @param subargs        Argument table for the subparser.
 * @param handle         Function pointer to the subcommand handler.
 * @param buffer         Pointer to a buffer described by data_layout
 * @param size           Size of the buffer
 * @param help           Optional help string for this subparser.
 * @param data_layout    Pointer to custom data layout.
 * @param layout_size    Size of the data layout array.
 *
 * @return A fully constructed ::aparse_arg definition representing the subparser.
 *
 * @note The parser core ignores this type during matching, but it is used
 *       by help and usage generators to represent subcommands.
 * @note If `ptr` is not provided, aparse will automatically allocate a buffer based on the provided layout, enabling seamless usage without manual memory setup.
 * @attention Generally recommended to use ::aparse_arg_subparser
 */
APARSE__INLINE aparse_arg aparse_arg_subparser_impl(
        const char* name,
        aparse_arg* subargs, 
        const aparse_handler_t handler,
        void *buffer, 
        const size_t size,
        const char* help, 
        const size_t *data_layout, 
        const size_t layout_size
) {
    return (aparse_arg) {
        .longopt = name, 
        .subargs = subargs, 
        .handler = handler,
        .data_layout = data_layout, 
        .layout_size = layout_size,
        .ptr = buffer, 
        .size = size,
        .help = help,
        .type = APARSE_ARG_TYPE_SUBPARSER 
    };
}

/**
 * @brief Create a root parser definition.
 *
 * Defines the top-level parser node that groups subparsers together.
 * This is used to construct hierarchical command structures.
 *
 * @param name        Name of the root parser (typically the program name).
 * @param subparsers  Array of subparser definitions (terminated with ::aparse_arg_end_marker).
 *
 * @return A constructed ::aparse_arg definition representing the root parser.
 */
APARSE__INLINE aparse_arg aparse_arg_parser(
        const char* name, 
        aparse_arg* subparsers) 
{
    return (aparse_arg){
        .longopt = name, 
        .subargs = subparsers, 
        .type = APARSE_ARG_TYPE_POSITIONAL
    };
}

/**
 * @brief Create an array argument definition.
 *
 * Defines a positional argument that stores multiple values (array behavior).
 * Useful for arguments that can appear multiple times or accept lists of items.
 *
 * @param name           Argument name.
 * @param dest           Pointer to the array where parsed values will be stored.
 * @param array_size     Total size of the destination array in bytes.
 * @param type           Element type (see ::aparse_arg_types).
 * @param element_size   Size of each array element in bytes (0 for pointer arrays).
 * @param help           Optional help string.
 *
 * @return A fully constructed ::aparse_arg definition representing an array argument.
 *
 * @note If @p element_size is 0, the parser assumes an array of pointers (`char*`).
 */
APARSE__INLINE aparse_arg aparse_arg_array(
        const char* name, 
        void* dest, 
        const size_t size, 
        const size_t array_size, 
        const aparse_arg_types type, 
        const size_t element_size, 
        const char* help) {
    return (aparse_arg){
        .longopt = name, .ptr = dest, .size = size,
        .array_size = array_size / (element_size == 0 ? sizeof(char*) : element_size),
        .type = 
            APARSE_ARG_TYPE_ARGUMENT | 
            APARSE_ARG_TYPE_ARRAY | 
            APARSE_ARG_TYPE_POSITIONAL | 
            type,
        .help = help, .element_size = element_size
    };
}

/**
 * @brief End marker for argument definition tables.
 *
 * Used as a sentinel value to mark the end of an ::aparse_arg array.
 * The parser stops reading arguments when it encounters this marker.
 *
 * @code{.c}
 * aparse_arg args[] = {
 *     aparse_arg_string("file", &file, sizeof(file), "Input file"),
 *     aparse_arg_number("count", &count, sizeof(count), APARSE_ARG_TYPE_INT, "Number of items"),
 *     aparse_arg_end_marker
 * };
 * @endcode
 */
#define aparse_arg_end_marker (aparse_arg){0}

/**
 * @brief Check if the current aparse_arg was an end marker
 *
 * @param arg The aparse_arg to check
 *
 * @note This will only check for `longopt` and `shortopt`, all the remaining
 * information will be discarded
 */
static inline bool aparse_arg_nend(const aparse_arg* arg) {
    return arg->longopt != 0 || arg->shortopt != 0;
}

/**
 * @brief Parse command-line arguments.
 *
 * Parses command-line input (`argc` / `argv`) according to a provided
 * argument table. Supports short and long options, positional arguments,
 * and subcommands.
 *
 * @param argc              Argument count (from `main`).
 * @param argv              Argument vector (from `main`).
 * @param args              Argument definition table, terminated with ::aparse_arg_end_marker.
 * @param dispatch_list_out Optional output for the list of dispatched function
 * @param program_desc      Optional program description for `--help` output (may be NULL).
 *
 * @return One of the ::aparse_status codes, typically ::APARSE_STATUS_OK on success.
 *
 * @note Errors and warnings can be intercepted using ::aparse_set_error_callback.
 * @note If `dispatch_list == NULL`, dispatched function will be executed immedieately after parsing complete
 */
aparse_status aparse_parse(
        const int argc, 
        char* const * argv, 
        aparse_arg* args, 
        aparse_list* dispatch_list_out, 
        const char* program_desc
);

/**
 * @brief Dispatch all queued handle
 *
 * Dispatch all handle with their respective constructed payload, then
 * also freeing any resources related to payload
 *
 * @param dispatch_list The list of dispatched functions
 */
extern void aparse_dispatch_all(aparse_list* dispatch_list);

/**
 * @brief Check for the handle inside dispatch list
 *
 * Check if the handle inside dispatch list was existed with given name,
 * normally it will be compared against `aparse_arg.longopt`
 *
 * @param name Name of dispatch handle to find
 *
 * @return If it wasn't existed in `dispatch_list`, return 1, otherwise return 0
 */
extern int aparse_dispatch_contain(const aparse_list* dispatch_list, const char* name);

/**
 * @brief Free a dispatch list without executing handlers
 *
 * Releases all resources associated with the dispatch list and its queued
 * handlers without invoking any handler functions. Any constructed payloads
 * stored in the list are freed.
 *
 * This function is typically used when argument parsing fails or when
 * execution of dispatched handlers is intentionally skipped.
 *
 * @param dispatch_list List of queued dispatch handlers to be freed
 */
extern void aparse_dispatch_free(aparse_list* dispatch_list);

/**
 * @brief Set a global error callback for parser events.
 *
 * Registers a callback function that is called whenever the parser
 * encounters an error or warning during parsing.
 *
 * @param cb        Pointer to a callback function of type ::aparse_error_callback.
 * @param userdata  User-defined pointer passed to the callback on each invocation.
 *
 * @note Passing `NULL` as @p cb using the library default callback.
 */
void aparse_set_error_callback(
        const aparse_error_callback cb, 
        void* userdata);

/**
 * @brief Returns a human-readable error message forstatus code.
 * @param status The status code.
 * @return A pointer to a constant null-terminated string if existed,
 *         otherwise a "Unknown error" string
 *
 * @warning The returned pointer must not be modified or freed.
 */
const char* aparse_error_msg(const aparse_status status);

#ifdef __cplusplus 
}
#endif

#endif
