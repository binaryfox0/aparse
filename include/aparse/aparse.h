#ifndef APARSE_H
#define APARSE_H

#include <stddef.h>

/* =========================================================================
 * Configuration constants and macros
 * ========================================================================= */

#define APARSE_MAX_DEPTH          8
#define APARSE_STRING_UNBOUNDED   0

#define APARSE__NARGS_SHIFT       (sizeof(size_t) * 8 - 2)

#define APARSE_NARGS_MODE_MASK    ((size_t)0x3 << APARSE__NARGS_SHIFT)
#define APARSE_NARGS_NONE         0
#define APARSE_NARGS_EXACT        1
#define APARSE_NARGS_MINIMUM      2
#define APARSE_NARGS_MAXIMUM      3

#define APARSE_NARGS_MODE(n)      (((n) & APARSE_NARGS_MODE_MASK) >> APARSE__NARGS_SHIFT)
#define APARSE_NARGS_COUNT(n)     ((n) & ~APARSE_NARGS_MODE_MASK)

#define APARSE__STATIC_ASSERT(c, m) \
    ((void)sizeof(char[(c) ? 1 : -1]), (m))

#define APARSE_NARGS(m, v) \
    (APARSE__STATIC_ASSERT((m) <= APARSE_NARGS_MAXIMUM, "APARSE_NARGS: invalid mode"), \
     APARSE__STATIC_ASSERT((v) <= ~APARSE_NARGS_MODE_MASK, "APARSE_NARGS: value exceeds available bits"), \
     (((size_t)(m) << APARSE__NARGS_SHIFT) | (size_t)(v)))

#define APARSE_PTR(type, value) (type[1]){(value)}


/* =========================================================================
 * Allocator and dynamic array
 * ========================================================================= */

/*
 * Custom allocator callbacks.
 *
 * All callbacks receive the allocator's userdata pointer.
 * The userdata must remain valid until all allocations made through this
 * allocator have been released.
 */
typedef void *(*aparse_malloc_fn)(void *userdata, size_t size);
typedef void *(*aparse_realloc_fn)(void *userdata, void *ptr, size_t size);
typedef void (*aparse_free_fn)(void *userdata, void *ptr);

typedef struct
{
    aparse_malloc_fn malloc;
    aparse_realloc_fn realloc;
    aparse_free_fn free;
    void *userdata;
} aparse_alloc_t;

typedef struct
{
    void *data;
    size_t size;
    size_t capacity;
} aparse_array_t;


/* =========================================================================
 * Argument types, kinds, and actions
 * ========================================================================= */

/* Type of value stored in an argument's destination. */
typedef enum
{
    APARSE_TYPE_STRING,
    APARSE_TYPE_INT,
    APARSE_TYPE_UINT,
    APARSE_TYPE_FLOAT,
    APARSE_TYPE_DOUBLE
} aparse_argument_type_t;

/* How an argument is interpreted on the command line. */
typedef enum
{
    APARSE_ARGUMENT_POSITIONAL,
    APARSE_ARGUMENT_FLAG,
    APARSE_ARGUMENT_OPTION,
    APARSE_ARGUMENT_COMMAND
} aparse_argument_kind_t;

typedef enum
{
    APARSE_ACTION_STORE = 0,
    APARSE_ACTION_APPEND,
    APARSE_ACTION_COUNT,
    /* store_true is the default action of boolean */
    APARSE_ACTION_STORE_TRUE = APARSE_ACTION_STORE,
    APARSE_ACTION_STORE_FALSE = 3
} aparse_action_t;


/* =========================================================================
 * Argument verification
 * ========================================================================= */

typedef struct aparse_argument aparse_argument_t;
typedef const char *aparse_verify_result_t;
typedef aparse_verify_result_t (*aparse_verify_fn)(
        void *userdata,
        const aparse_argument_t *arg,
        const void *value);

typedef struct
{
    aparse_verify_fn verify_fn;
    void *userdata;
} aparse_verify_t;


/* =========================================================================
 * Argument definition
 * ========================================================================= */

/*
 * Describes an argument or subcommand.
 *
 * Definitions are borrowed by the parser and must remain valid for the
 * duration of parsing.
 *
 * The parser does not modify argument definitions.
 *
 * longopt and shortopt are strings, allowing users to choose their
 * preferred option prefixes, e.g. "--" or "- or "/".
 *
 * ptr points to the destination where the parsed value is stored.
 * size describes the destination storage size.
 */
struct aparse_argument
{
    aparse_argument_kind_t kind;
    aparse_argument_type_t type;
    aparse_action_t action;

    const char *longopt;
    const char *shortopt;
    const char *help;
    const char *metavar;

    const void *default_val;
    void *out_val;
    size_t size;
    size_t nargs;
    aparse_verify_t verify;

    const struct aparse_argument *children;
    size_t children_count;
};


/* =========================================================================
 * Parser configuration
 * ========================================================================= */

typedef struct
{
    const char *shortopt;
    const char *longopt;
} aparse_action_trigger_t;

/*
 * Parser configuration.
 *
 * If config is NULL, the default system allocator is used.
 *
 * If a custom allocator is provided, all three callbacks must be valid.
 */
typedef struct
{
    aparse_alloc_t alloc;
    aparse_action_trigger_t help;
    aparse_action_trigger_t version;
} aparse_config_t;

extern const aparse_config_t aparse_default_config;


/* =========================================================================
 * User input errors
 * ========================================================================= */

/*
 * Describes the reason parsing stopped due to command-line input.
 *
 * These values are reported through aparse_result_t.
 *
 * APARSE_UERR_OK indicates that no user-level condition was reported.
 *
 * APARSE_UERR_HELP indicates that the user requested help. It is a
 * control-flow outcome, not an invalid-input error.
 */
typedef enum
{
    APARSE_UERR_OK,

    APARSE_UERR_HELP,
    APARSE_UERR_VERSION,

    /* Option errors */
    APARSE_UERR_MISSING_VALUE,
    APARSE_UERR_INVALID_VALUE,
    APARSE_UERR_VALUE_OVERFLOW,
    APARSE_UERR_VALUE_UNDERFLOW,
    APARSE_UERR_VERIFICATION,

    /* Argument errors */
    APARSE_UERR_UNEXPECTED_ARGUMENT,
    APARSE_UERR_MISSING_POSITIONAL,

    /* Command errors */
    APARSE_UERR_UNKNOWN_COMMAND,
    APARSE_UERR_MISSING_COMMAND,

    APARSE__UERR_COUNT
} aparse_user_error_t;


/* =========================================================================
 * Library status codes
 * ========================================================================= */

/*
 * Return status of parser operations.
 *
 * APARSE_ERR_OK:
 *     Parsing completed successfully.
 *
 * APARSE_ERR_HELP:
 *     A help request was encountered. This is a control-flow status,
 *     not an actual error. Parsing stops and the status propagates through
 *     nested parsing layers.
 *
 * APARSE_ERR_USER:
 *     Command-line input was invalid. Detailed information is available
 *     through aparse_result_t.
 *
 * Other APARSE_ERR_* values:
 *     A library, API, resource, or internal failure occurred.
 *
 * Result contract:
 *
 *     APARSE_ERR_OK:
 *         result->user_error == APARSE_UERR_OK
 *
 *     APARSE_ERR_HELP:
 *         result->user_error == APARSE_UERR_HELP
 *
 *     APARSE_ERR_USER:
 *         result->user_error != APARSE_UERR_OK
 *         and result->user_error != APARSE_UERR_HELP
 *
 *     Other errors:
 *         Result diagnostic fields are not guaranteed to be populated.
 */
typedef enum
{
    APARSE_ERR_OK = 0,

    /* API usage errors */
    APARSE_ERR_UNIMPLEMENTED,
    APARSE_ERR_INVALID_ARGUMENT,

    /* Argument definition errors */
    APARSE_ERR_INVALID_SPEC,
    APARSE_ERR_DUPLICATE_NAME,
    APARSE_ERR_INVALID_TYPE,
    APARSE_ERR_INVALID_SIZE,
    APARSE_ERR_INVALID_DESTINATION,
    APARSE_ERR_TOO_DEEP,

    /* Resource errors */
    APARSE_ERR_OUT_OF_MEMORY,
    APARSE_ERR_SIZE_OVERFLOW,

    /* Input/source handling errors */
    APARSE_ERR_INVALID_CMDLINE,
    APARSE_ERR_INVALID_ENCODING,

    /* User input errors */
    APARSE_ERR_USER,

    /* Internal errors */
    APARSE_ERR_INTERNAL,

    APARSE__ERR_COUNT
} aparse_error_t;


/* =========================================================================
 * Parse result types
 * ========================================================================= */

/*
 * Represents the selected subcommand hierarchy.
 *
 * Each node represents a selected command. Its subcommand points to the
 * next selected command, if any.
 *
 * command points to the corresponding argument definition.
 *
 * The result tree is owned by aparse_result_t and must not be freed
 * separately.
 */
typedef struct aparse_subcommand_node
{
    struct aparse_subcommand_node *next;
    const aparse_argument_t *command;
} aparse_subcommand_node_t;

typedef struct
{
    const aparse_argument_t *args;
    size_t args_count;
} aparse_stack_frame_t;


/*
 * Stores information produced during parsing.
 *
 * Dynamically allocated result storage is owned by the parser.
 * Call aparse_destroy_result() to release it.
 *
 * Argument definitions and input strings are borrowed.
 *
 * user_error:
 *     APARSE_UERR_OK means no user-level condition occurred.
 *     APARSE_UERR_HELP means help was requested.
 *     Other values describe invalid command-line input.
 *
 * scope_path:
 *     Path to the command scope containing the argument that triggered
 *     the error.
 *
 * arg:
 *     Argument definition associated with the condition, if applicable.
 *
 * strings:
 *     Additional strings associated with the condition. Their meaning
 *     depends on user_error.
 *
 * value:
 *     Input value associated with the condition, if applicable.
 */
typedef struct
{
    /* Errors */
    aparse_user_error_t user_error;
    const char *verification_error;
    size_t expected_count;

    /* Argument context */
    const aparse_argument_t *arg;
    size_t arg_count;

    /* Scope / execution stack */
    aparse_stack_frame_t stack[APARSE_MAX_DEPTH];
    size_t stack_frame_count;

    /* Parsed strings */
    const char **strings;
    size_t strings_count;
    size_t strings_capacity;

    /* Parsed value */
    const char *value;

    /* Subcommands */
    aparse_subcommand_node_t *subcommand_head;
} aparse_result_t;

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Error strings
 * ========================================================================= */

/* Returns a description of a library status code. */
const char *aparse_strerror(const aparse_error_t err);

/* Returns a description of a user-level condition. */
const char *aparse_user_strerror(const aparse_user_error_t err);


/* =========================================================================
 * Parsing
 * ========================================================================= */

/*
 * Retrieves the executable name from the process command line.
 *
 * out_buf:
 *     Destination buffer.
 *
 * size:
 *     Capacity of the destination buffer.
 */
void aparse_get_program_name(
        char *out_buf,
        const size_t size);


/*
 * Parses command-line arguments from argv.
 *
 * argc, argv:
 *     Command-line arguments. Input strings are borrowed and must remain
 *     valid while the result is being inspected.
 *
 * args, args_count:
 *     Argument definitions for the root command.
 *
 * config:
 *     Optional parser configuration. NULL selects the default allocator.
 *
 * result:
 *     Receives parsing results and diagnostics.
 *
 * Returns:
 *     APARSE_ERR_OK on successful parsing.
 *     APARSE_ERR_HELP when help was requested.
 *     APARSE_ERR_USER when command-line input is invalid.
 *     Another APARSE_ERR_* value on library/API failure.
 *
 * The result must be released with aparse_destroy_result() when no longer
 * needed, provided it was initialized according to the implementation's
 * result-initialization contract.
 */
aparse_error_t aparse_parse_from_argv(
        const int argc,
        char *const *argv,
        const aparse_argument_t *args,
        const size_t args_count,
        const aparse_config_t *config,
        aparse_result_t *result);


/*
 * Parses arguments from a command-line string.
 *
 * The command-line string is borrowed and must remain valid while the
 * result is being inspected.
 *
 * Return-value and result contracts are identical to
 * aparse_parse_from_argv().
 */
aparse_error_t aparse_parse_from_cmdline(
        const char *cmdline,
        const size_t cmdline_len,
        const aparse_argument_t *args,
        const size_t args_count,
        const aparse_config_t *config,
        aparse_result_t *result);


/* =========================================================================
 * Output
 * ========================================================================= */

/* Prints usage information for the selected command. */
void aparse_print_usage(
        const aparse_result_t *result);

/* Prints user-input diagnostics from a parse result. */
void aparse_print_error(
        const aparse_result_t *result);

/* Prints help for the specified command. */
void aparse_print_help(
        const aparse_result_t *result);


/* =========================================================================
 * Cleanup
 * ========================================================================= */

/* Releases storage owned by a dynamic array. */
void aparse_destroy_array(
        aparse_array_t *array,
        const aparse_config_t *config);

/*
 * Releases all memory owned by the result.
 *
 * Must be called once for every initialized result.
 */
void aparse_destroy_result(
        aparse_result_t *result,
        const aparse_config_t *config);

#ifdef __cplusplus 
}
#endif

#endif
