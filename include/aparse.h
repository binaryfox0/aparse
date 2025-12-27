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

#include "aparse_list.h" // IWYU pragma: export

/**
 * @def APARSE_INLINE
 * @brief Cross-compiler inline function specifier.
 *
 * This macro ensures that functions are always inlined when possible,
 * using compiler-specific attributes.
 *
 * - On **MSVC**, it expands to `__forceinline`.
 * - On **non-MSVC compilers** (GCC, Clang, etc.), it expands to
 *   `static inline __attribute__((always_inline))`.
 *
 * @note
 * This macro should be used in internal headers or performance-critical
 * functions where inlining is required for efficiency.
 *
 * @warning
 * Forcing inlining may increase binary size if used excessively.
 *
 * @see https://learn.microsoft.com/en-us/cpp/cpp/inline-functions-cpp  
 * @see https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-always_005finline-function-attribute
 */
#if defined(_MSC_VER)
#   define APARSE_INLINE __forceinline
#else
#   define APARSE_INLINE static inline __attribute__((always_inline))
#endif


/** 
 * @defgroup aparse_macros aparse's helper macros
 * @brief Macros to define parser arguments and subparsers.
 *
 * This module provides macros to simplify creating @ref aparse_arg_s entries,
 * automatically generating offset-size layouts, counting arguments, and mapping
 * functions over variadic arguments.
 *
 * @{
 */

#if _MSVC_TRADITIONAL == 0 || __GNUC__ || __clang__
#   define __APARSE_VA_ARGS_EXPANSION_CONFORM

/** @cond HIDDEN */

#   define __aparse_cat_impl(a, b) a##b

#   define __aparse_count_args_impl( \
        _1, _2, _3, _4, _5, _6, _7, _8, \
        _9, _10,_11,_12,_13,_14,_15,_16,N,...) N

#   define __aparse_map_1(m, a, x)               m(a, x)
#   define __aparse_map_2(m, a, x, ...)          m(a, x), __aparse_map_1(m, a, __VA_ARGS__)
#   define __aparse_map_3(m, a, x, ...)          m(a, x), __aparse_map_2(m, a, __VA_ARGS__)
#   define __aparse_map_4(m, a, x, ...)          m(a, x), __aparse_map_3(m, a, __VA_ARGS__)
#   define __aparse_map_5(m, a, x, ...)          m(a, x), __aparse_map_4(m, a, __VA_ARGS__)
#   define __aparse_map_6(m, a, x, ...)          m(a, x), __aparse_map_5(m, a, __VA_ARGS__)
#   define __aparse_map_7(m, a, x, ...)          m(a, x), __aparse_map_6(m, a, __VA_ARGS__)
#   define __aparse_map_8(m, a, x, ...)          m(a, x), __aparse_map_7(m, a, __VA_ARGS__)
#   define __aparse_map_9(m, a, x, ...)          m(a, x), __aparse_map_8(m, a, __VA_ARGS__)
#   define __aparse_map_10(m, a, x, ...)         m(a, x), __aparse_map_9(m, a, __VA_ARGS__)
#   define __aparse_map_11(m, a, x, ...)         m(a, x), __aparse_map_10(m, a, __VA_ARGS__)
#   define __aparse_map_12(m, a, x, ...)         m(a, x), __aparse_map_11(m, a, __VA_ARGS__)
#   define __aparse_map_13(m, a, x, ...)         m(a, x), __aparse_map_12(m, a, __VA_ARGS__)
#   define __aparse_map_14(m, a, x, ...)         m(a, x), __aparse_map_13(m, a, __VA_ARGS__)
#   define __aparse_map_15(m, a, x, ...)         m(a, x), __aparse_map_14(m, a, __VA_ARGS__)
#   define __aparse_map_16(m, a, x, ...)         m(a, x), __aparse_map_15(m, a, __VA_ARGS__)

#   define __aparse_offsetof_and_sizeof(s, m) offsetof(s, m), sizeof(((s*)0)->m )
#   define __aparse_offsetofs(s, ...) { __aparse_expand(__aparse_map(__aparse_offsetof_and_sizeof, s, __VA_ARGS__)) }
/** @endcond */

/**
 * @brief Expands the arguments passed to a variadic macro.
 *
 * This macro simply returns its input list of arguments. It is useful
 * when forcing Doxygen or the preprocessor to fully expand nested variadic macros.
 *
 * @param ... Variadic arguments to expand.
 */
#   define __aparse_expand(...) __VA_ARGS__

/**
 * @brief Concatenate two tokens after macro expansion.
 *
 * This macro ensures proper token pasting in complex macro expressions.
 *
 * @param a First token
 * @param b Second token
 * @return Concatenation of a and b
 */
#   define __aparse_cat(a, b) __aparse_cat_impl(a, b)

/**
 * @brief Count the number of arguments passed to a variadic macro (up to 16).
 *
 * This macro uses a helper implementation to determine the number of arguments
 * in a variadic macro call.
 *
 * @param ... Variadic list of arguments
 * @return Number of arguments passed
 */
#   define __aparse_count_args(...) __aparse_count_args_impl(__VA_ARGS__, \
        16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1)

/**
 * @brief Map a macro `m(a, x)` over a variadic list of arguments.
 *
 * This is the public-facing helper; internally it expands to `__map_1` .. `__map_16`.
 * 
 * @param m Macro to apply to each element (must take two arguments: `a` and `x`)
 * @param a Extra argument passed to `m`
 * @param ... Variadic list of items to process
 */
#   define __aparse_map(m, a, ...) __aparse_cat(__aparse_map_, __aparse_count_args(__VA_ARGS__))(m, a, __VA_ARGS__)

/**
 * @brief Create a subparser argument entry for @ref aparse_arg_s.
 *
 * This macro wraps `aparse_arg_subparser_impl` and automatically generates
 * the `data_layout` array for the specified members of `data_struct`.
 *
 * @param name        The argument name (string).
 * @param subargs     Pointer to subparser argument array.
 * @param handle      Function pointer to handler called after parsing.
 * @param help        Help string describing this subparser.
 * @param data_struct Struct type containing members to map.
 * @param ...         List of member names of `data_struct` to include in the layout.
 *
 * @note Uses compiler-specific variadic macro expansions; some older compilers
 *       may not support it correctly.
 *
 * @example
 * @code{.c}
 * struct config {
 *     int port;
 *     char *name;
 * };
 * aparse_arg_subparser("config", config_subargs, handle_config, "Config subparser", config, port, name);
 * @endcode
 */
#   define aparse_arg_subparser(name, subargs, handle, help, data_struct, ...) \
       aparse_arg_subparser_impl(name, subargs, handle, help, (int[])__aparse_offsetofs(data_struct, __VA_ARGS__), __aparse_count_args(__VA_ARGS__))
#else
#   pragma message("Warning: This compiler wasn't conformed to __VA_ARGS__ in C standard")
#endif

/** @} */ // end of aparse_macros

/** @cond HIDDEN */

// A trick to deal with Microsoft Visual Studio Compiler macro expansion (indirection trick)
#define __aparse_printf_impl(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define __aparse_printf(fmt, ...) __aparse_printf_impl(fmt, ##__VA_ARGS__)
#define __aparse_fprintf_impl(fp, fmt, ...) fprintf(fp, fmt, ##__VA_ARGS__)
#define __aparse_fprintf(fp, fmt, ...) __aparse_fprintf_impl(fp, fmt, ##__VA_ARGS__)

/** @endcond */

/**
 * @brief Print informational message to stdout, with color if supported
 *
 * @param fmt to a null-terminated multibyte string specifying how to interpret the data
 * @param ... specifying data to print. Undefined behaviour if type mismatched, discarded if extraneous
 *
 * @note Should be used after calling \ref aparse_parse, otherwise program name will be `(null)`
 */
#define aparse_prog_info(fmt, ...) __aparse_printf("%s: " __aparse_ansies("\x1b[1;34m") "info" __aparse_ansies("\x1b[0m") ": " fmt "\n", __aparse_progname, ##__VA_ARGS__)

/**
 * @brief Print warning message to stdout, with color if supported
 *
 * @param fmt to a null-terminated multibyte string specifying how to interpret the data
 * @param ... specifying data to print. Undefined behaviour if type mismatched, discarded if extraneous
 *
 * @note Should be used after calling \ref aparse_parse, otherwise program name will be `(null)`
 */
#define aparse_prog_warn(fmt, ...) __aparse_printf("%s: " __aparse_ansies("\x1b[1;33m") "warn" __aparse_ansies("\x1b[0m") ": " fmt "\n", __aparse_progname, ##__VA_ARGS__)

/**
 * @brief Print error message to stdout, with color if supported
 *
 * @param fmt to a null-terminated multibyte string specifying how to interpret the data
 * @param ... specifying data to print. Undefined behaviour if type mismatched, discarded if extraneous
 *
 * @note Should be used after calling \ref aparse_parse, otherwise program name will be `(null)`
 */
#define aparse_prog_error(fmt, ...) __aparse_fprintf(stderr, "%s: "  __aparse_ansies("\x1b[1;31m") "error" __aparse_ansies("\x1b[0m") ": " fmt "\n", __aparse_progname, ##__VA_ARGS__)


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum aparse_arg_types_e
 * @brief Enumerates all possible argument types recognized by the parser.
 *
 * These values define how each @ref aparse_arg_s entry should be interpreted.
 * Some constants are combinations or modifiers of others (bitmask-style flags).
 *
 * @see aparse_arg_s
 */
enum aparse_arg_types_e
{
    /**
     * @brief Sign flag mask for signed argument types.
     *
     * Used internally to mark that an argument type is signed.
     */
    APARSE_ARG_TYPE_SIGNED_FLAGS = (1 << 7),

    /**
     * @brief Unknown or uninitialized argument type.
     */
    APARSE_ARG_UNKNOWN = 0,

    /**
     * @brief String argument type.
     *
     * Stores a pointer to a null-terminated string.
     */
    APARSE_ARG_TYPE_STRING,

    /**
     * @brief Boolean argument type.
     *
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
     *
     * Indicates that the argument points to a list of values rather than a single one.
     */
    APARSE_ARG_TYPE_ARRAY = (1 << 3),

    /**
     * @brief Positional argument type.
     *
     * Used for arguments that are identified by their position, not by an option name.
     */
    APARSE_ARG_TYPE_POSITIONAL = (1 << 4),

    /**
     * @brief Normal argument type.
     *
     * Used for standard named options like `--file` or `-f`.
     */
    APARSE_ARG_TYPE_ARGUMENT   = (1 << 5),

    /**
     * @brief Subparser or subcommand argument type.
     *
     * Equivalent to @ref APARSE_ARG_TYPE_POSITIONAL, but used for defining subcommands.
     */
    APARSE_ARG_TYPE_SUBPARSER  = APARSE_ARG_TYPE_POSITIONAL,

    /**
     * @brief Signed integer argument type.
     *
     * Combines @ref APARSE_ARG_TYPE_UNSIGNED with @ref APARSE_ARG_TYPE_SIGNED_FLAGS.
     */
    APARSE_ARG_TYPE_SIGNED = APARSE_ARG_TYPE_UNSIGNED | APARSE_ARG_TYPE_SIGNED_FLAGS,

    /**
     * @brief Bitmask used to extract base argument type.
     *
     * Can be applied to a type value to ignore modifier flags.
     */
    APARSE_ARG_TYPE_BITMASK = 0b111
};

/** @typedef aparse_arg_types
 * @brief Type alias for @ref aparse_arg_types_e.
 */
typedef enum aparse_arg_types_e aparse_arg_types;

/**
 * @struct aparse_arg_s
 * @brief Describes a single argument, option, or subparser definition.
 *
 * The `aparse_arg_s` structure defines the metadata for each command-line
 * argument or subparser, including its option names, data type, storage
 * pointer, and parsing behavior.
 *
 * The meaning of certain members depends on the argument type
 * (see @ref aparse_arg_types).
 */
struct aparse_arg_s 
{
    /**
     * @brief The short option name (e.g., "-h") for optional arguments.
     *
     * - Ignored for positional arguments.  
     * - May be `NULL` if `longopt` is used instead.
     */
    char* shortopt;

    /**
     * @brief The long option name (e.g., "--help") for optional arguments.
     *
     * - Required for positional arguments.  
     * - May be `NULL` if `shortopt` is present and sufficient.
     */
    char* longopt;

    /**
     * @brief A help message describing the argument.
     *
     * The message will be displayed in help outputs generated by the parser.
     * Can be NULL
     */
    char* help;

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
    int size;

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
             * @brief Pointer to the variable where the parsed value will be stored.
             */
            void* ptr;

            /**
             * @brief Size of each element for array arguments.
             */
            int element_size;
        };
        // For subparsers/subcommands
        struct {
            /**
             * @brief Array of subarguments used in the subcommand.
             */
            struct aparse_arg_s* subargs;

            /**
             * @brief Handler function called after successful subparser parsing.
             *
             * @param data Pointer to the data structure described by @ref data_layout.
             */
            void (*handler)(void* data);

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
            int* data_layout;
        };
    };
};

/** @typedef aparse_arg
 * @brief Type alias for @ref aparse_arg_s.
 */
typedef struct aparse_arg_s aparse_arg;

/**
 * @enum aparse_status_e
 * @brief Status codes returned by aparse operations or reported to error callbacks.
 */
enum aparse_status_e
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

    APARSE_STATUS_ALLOC_FAILURE,        /**< Memory allocation failed. */
    APARSE_STATUS_UNHANDLED,            /**< Unhandled type of argument. */
    __APARSE_STATUS_ENUM_END__          /**< The marker for the end of aparse_status. THIS MUST BE AT THE END */
};

/** @typedef aparse_status
 * @brief Type alias for @ref aparse_status_e.
 */
typedef enum aparse_status_e aparse_status;

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
 * | Status code                      | field1 type            | field2 type            | Description                                       |
 * |----------------------------------|------------------------|------------------------|---------------------------------------------------|
 * | ::APARSE_STATUS_UNKNOWN_ARGUMENT   | `unknown_args`         | `NULL`                 | Unknown argument name.                            |
 * | ::APARSE_STATUS_MISSING_VALUE      | `current_arg`          | `expected_count`       | Option definition that requires a value.          |
 * | ::APARSE_STATUS_INVALID_VALUE      | `current_arg`          | `current_argv`         | Argument definition and invalid value string.     |
 * | ::APARSE_STATUS_OVERFLOW           | `current_arg`          | `current_argv`         | Numeric argument and overflowing value string.    |
 * | ::APARSE_STATUS_UNDERFLOW          | `current_arg`          | `current_argv`         | Numeric argument and underflowing value string.   |
 * | ::APARSE_STATUS_MISSING_POSITIONAL | `current_arg`          | `required_args`        | Missing positional argument definition.           |
 * | ::APARSE_STATUS_INVALID_SUBCOMMAND | `parser_subargs`       | `current_argv`         | Invalid subcommand name.                          |
 * | ::APARSE_STATUS_NULL_POINTER       | `current_arg`          | `NULL`                 | Invalid NULL pointer in user argument definition. |
 * | ::APARSE_STATUS_INVALID_TYPE       | `current_arg`          | `NULL`                 | Type mismatch in argument definition.             |
 * | ::APARSE_STATUS_INVALID_SIZE       | `current_arg`          | `arg_size`             | Invalid size field in argument definition.        |
 * | ::APARSE_STATUS_ALLOC_FAILURE      | `NULL`                 | `NULL`                 | Memory allocation failed inside parser.           |
 * | ::APARSE_STATUS_UNHANDLED          | `current_arg`          | `NULL`                 | An unhandled type of argument.                    |
 *
 * - `const aparse_list* unknown_args  `: An aparse_list refer to a list of arguments. `unknown_args.ptr` should be converted into `aparse_arg*`
 * - `const aparse_arg*  current_arg   `: An aparse_arg* refer to the currently processed argument.
 * - `const int*         expected_count`: The count of expected argv of `current_arg`
 * - `const char*        cargv         `: The current argv is currently being processed
 * - `const aparse_list* required_args `: An aparse_list refer to a list of required arguments. `required_args.ptr` should be converted into `aparse_arg*`
 * - `const int*         size          `: The invalid size of `current_arg`. It can be `current_arg.size` or `current_arg.element_size`
 */
typedef void (*aparse_error_callback)(const aparse_status status, const void* field1, const void* field2, void *userdata);

/** @cond INTERNAL */
extern const char* __aparse_progname;
/** @endcond */

#if defined(_WIN32)
/**
 * @def APARSE_PLATFORM_WIN32
 * @brief Platform detection macro for Windows systems.
 *
 * This macro is defined automatically when `_WIN32` is detected,
 * indicating that the build is targeting a Windows environment.
 *
 * It is used internally to enable Windows-specific code paths, such as
 * header inclusion (`<sdkddkver.h>`) or feature checks (e.g., `NTDDI_VERSION`).
 *
 * @note
 * - `_WIN32` is defined for both 32-bit and 64-bit Windows builds.
 * - This macro is not defined on Unix-like or POSIX platforms.
 *
 * @see https://learn.microsoft.com/en-us/cpp/preprocessor/predefined-macros
 */

#   define APARSE_PLATFORM_WIN32
#endif

#ifdef APARSE_PLATFORM_WIN32
#   include <sdkddkver.h>
// Source:
// - https://learn.microsoft.com/en-us/windows/win32/winprog/using-the-windows-headers?redirectedfrom=MSDN#macros-for-conditional-declarations
// - https://en.wikipedia.org/wiki/ANSI_escape_code#DOS_and_Windows 
#   if defined(NTDDI_VERSION) && (NTDDI_VERSION >= NTDDI_WIN10_TH2)
/**
 * @brief Conditionally enables ANSI escape sequences on supported platforms.
 *
 * This macro is used to wrap ANSI color or style escape codes in a way
 * that prevents them from being emitted on Windows systems that do not
 * support virtual terminal sequences.
 *
 * On Unix-like systems, or on Windows 10 build 10586 (Version 1511, "TH2")
 * and newer, `__aparse_ansies(str)` expands to `str` directly.
 * On older Windows versions, it expands to nothing, effectively disabling
 * colored output.
 *
 * @param str The ANSI escape sequence string to include, e.g. `"\x1b[1;31m"`.
 *
 * @note
 * This macro uses `_WIN32` and `NTDDI_VERSION` to determine availability:
 * - If `_WIN32` is not defined → ANSI escape codes are always enabled.
 * - If `_WIN32` is defined but `NTDDI_VERSION < NTDDI_WIN10_TH2` → disabled.
 * - If `_WIN32` and `NTDDI_VERSION >= NTDDI_WIN10_TH2` → enabled.
 *
 * @see https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
 * @see https://en.wikipedia.org/wiki/ANSI_escape_code#DOS_and_Windows
 */
#       define __aparse_ansies(str) str
#   endif
#else
#   define __aparse_ansies(str) str
#endif

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
APARSE_INLINE aparse_arg aparse_arg_option(char* shortopt, char* longopt, void* dest, int size, aparse_arg_types type, char* help) {
    return (aparse_arg){
        .shortopt = shortopt, .longopt = longopt,
        .type = type | APARSE_ARG_TYPE_ARGUMENT, 
        .ptr = dest, .size = size, .help = help,
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
APARSE_INLINE aparse_arg aparse_arg_number(char* name, void* dest, int size, aparse_arg_types type, char* help) {
    return (aparse_arg){
        .longopt = name, .ptr = dest, .size = size, 
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
APARSE_INLINE aparse_arg aparse_arg_string(char* name, void* dest, int size, char* help) {
    return (aparse_arg) {
        .longopt = name, .ptr = dest, .size=size,
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
 * @param name          Subcommand name (e.g. "build", "test").
 * @param subargs        Argument table for the subparser.
 * @param handle         Function pointer to the subcommand handler.
 * @param help           Optional help string for this subparser.
 * @param data_layout    Pointer to custom data layout.
 * @param layout_size    Size of the data layout array.
 *
 * @return A fully constructed ::aparse_arg definition representing the subparser.
 *
 * @note The parser core ignores this type during matching, but it is used
 *       by help and usage generators to represent subcommands.
 * @attention Generally recommended to use ::aparse_arg_subparser
 */
APARSE_INLINE aparse_arg aparse_arg_subparser_impl(
    char* name,aparse_arg* subargs, void (*handle)(void*), 
    char* help, int* data_layout, int layout_size
) {
    return (aparse_arg) {
        .longopt = name, .subargs = subargs, .handler = handle,
        .data_layout = data_layout, .size = layout_size,
        .help = help,
        .type = APARSE_ARG_TYPE_POSITIONAL // parser doesn't care 'bout this, but help constructor DOES care
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
APARSE_INLINE aparse_arg aparse_arg_parser(char* name, aparse_arg* subparsers) {
    return (aparse_arg){.longopt = name, .subargs = subparsers, .type = APARSE_ARG_TYPE_POSITIONAL};
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
APARSE_INLINE aparse_arg aparse_arg_array(char* name, void* dest, int array_size, aparse_arg_types type, int element_size, char* help) {
    return (aparse_arg){
        .longopt = name, .ptr = dest, .size = array_size / (element_size == 0 ? sizeof(char*) : element_size),
        .type = APARSE_ARG_TYPE_ARGUMENT | APARSE_ARG_TYPE_ARRAY | APARSE_ARG_TYPE_POSITIONAL | type,
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
extern int aparse_parse(const int argc, char* const * argv, aparse_arg* args, aparse_list* dispatch_list_out, const char* program_desc);

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
extern void aparse_set_error_callback(const aparse_error_callback cb, void* userdata);

extern const char* aparse_error_msg(const aparse_status status);

#ifdef __cplusplus 
}
#endif

#endif
