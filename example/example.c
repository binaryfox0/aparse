#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <aparse/aparse.h>

#define ARRSZ(x) (sizeof(x) / sizeof((x)[0]))
#define STR_OR_NULL(x) ((x) ? (x) : "(null)")

/* -------------------------------------------------------------------------
 * Verification helpers (used to implement argparse-style choices).
 * ------------------------------------------------------------------------- */

typedef struct
{
    const char *const *values;
    size_t count;
    const char *error;
} choice_set_t;

static aparse_verify_result_t verify_uint_range(
        void *userdata,
        const aparse_argument_t *argument,
        const void *value)
{
    const uint32_t number = *(const uint32_t *)value;
    (void)userdata;
    (void)argument;

    if(number > 3)
        return "expected optimization level from 0 to 3";

    return NULL;
}

static aparse_verify_result_t verify_string_choice(
        void *userdata,
        const aparse_argument_t *argument,
        const void *value)
{
    const choice_set_t *choices = (const choice_set_t *)userdata;
    const char *const string = *(char *const *)value;
    size_t i;

    (void)argument;

    for(i = 0; i < choices->count; i++)
    {
        const char *candidate = choices->values[i];
        const char *a = string;
        const char *b = candidate;

        while(*a && *a == *b)
        {
            a++;
            b++;
        }

        if(*a == '\0' && *b == '\0')
            return NULL;
    }

    return choices->error;
}

static void print_string_array(const char *label, const aparse_array_t *array)
{
    size_t i;
    const char *const *items = (const char *const *)array->data;

    printf("%s:\n", label);
    for(i = 0; i < array->size; i++)
        printf("  - %s\n", STR_OR_NULL(items[i]));
}

static void print_help_description(
        const aparse_result_t *result,
        const aparse_argument_t *root_args,
        const aparse_argument_t *build_args)
{
    const aparse_stack_frame_t *frame;

    if(result->stack_frame_count == 0)
        return;

    frame = &result->stack[result->stack_frame_count - 1];

    /* argparse description is not currently part of aparse_argument_t.
     * Print the two descriptions used by this reference example manually.
     */
    if(frame->args == root_args)
        fprintf(stderr, "Reference CLI for testing aparse features.\n");
    else if(frame->args == build_args)
        fprintf(stderr, "Build the project using the selected configuration.\n");
}

int main(int argc, char **argv)
{
    /* Root options. */
    uint32_t verbosity = 0;
    bool quiet = false;
    char *config_file = NULL;
    char *color = NULL;
    bool debug = false;
    bool release = false;

    /* Shared build-command options. */
    bool no_color = false;

    /* build */
    char *source = NULL;
    char *output = NULL;
    uint32_t jobs = 1;
    uint32_t optimization = 2;
    bool fast_math = false;
    bool warnings_as_errors = false;
    aparse_array_t definitions = {0};
    aparse_array_t include_dirs = {0};
    aparse_array_t features = {0};

    /* clean */
    bool clean_all = false;
    bool clean_dry_run = false;

    /* network list */
    bool network_all = false;
    char *network_format = NULL;
    uint32_t network_limit = 0;

    /* network add */
    char *interface_name_add = NULL;
    char *address = NULL;
    uint32_t prefix = 24;
    char *gateway = NULL;
    bool enabled = false;

    /* network remove */
    char *interface_name_remove = NULL;
    bool force = false;

    /* service list */
    bool service_running = false;
    char *service_list_format = NULL;

    /* service start / stop */
    char *service_name_start = NULL;
    char *service_name_stop = NULL;
    double start_timeout = 30.0;
    double stop_timeout = 30.0;
    bool wait = false;

    static const char *const color_values[] = {"auto", "always", "never"};
    static const char *const network_formats[] = {"table", "json", "yaml"};
    static const char *const service_formats[] = {"table", "json"};
    static const choice_set_t color_choices = {
        color_values, ARRSZ(color_values),
        "expected color policy: auto, always, or never"
    };
    static const choice_set_t network_format_choices = {
        network_formats, ARRSZ(network_formats),
        "expected format: table, json, or yaml"
    };
    static const choice_set_t service_format_choices = {
        service_formats, ARRSZ(service_formats),
        "expected format: table or json"
    };

    /* ---------------------------------------------------------------------
     * build arguments
     * --------------------------------------------------------------------- */

    const aparse_argument_t build_args[] = {
        APARSE_EZ_COUNT_UINT(
            "-v", "--verbose", &verbosity,
            "Increase verbosity"),
        APARSE_EZ_FLAG(
            "-q", "--quiet", &quiet,
            "Suppress non-error output"),
        APARSE_EZ_FLAG(
            NULL, "--no-color", &no_color,
            "Disable colored output"),

        APARSE_EZ_POSITIONAL_STRING(
            "SOURCE", &source,
            "Source directory"),

        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_STRING,
            .action = APARSE_ACTION_STORE,
            .shortopt = "-o",
            .longopt = "--output",
            .metavar = "DIR",
            .out_val = &output,
            .size = APARSE_STRING_UNBOUNDED,
            .nargs = APARSE_NARGS(APARSE_NARGS_EXACT, 1),
            .default_val = APARSE_PTR(char *, "build"),
            .help = "Output directory"
        },
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_UINT,
            .action = APARSE_ACTION_STORE,
            .shortopt = "-j",
            .longopt = "--jobs",
            .metavar = "N",
            .out_val = &jobs,
            .size = sizeof(jobs),
            .nargs = APARSE_NARGS(APARSE_NARGS_EXACT, 1),
            .default_val = APARSE_PTR(uint32_t, 1),
            .help = "Number of parallel jobs"
        },
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_UINT,
            .action = APARSE_ACTION_STORE,
            .longopt = "--optimization",
            .metavar = "LEVEL",
            .out_val = &optimization,
            .size = sizeof(optimization),
            .nargs = APARSE_NARGS(APARSE_NARGS_EXACT, 1),
            .default_val = APARSE_PTR(uint32_t, 2),
            .verify = { .verify_fn = verify_uint_range },
            .help = "Optimization level (0-3)"
        },
        APARSE_EZ_FLAG(
            NULL, "--fast-math", &fast_math,
            "Enable fast floating-point math"),
        APARSE_EZ_FLAG(
            NULL, "--warnings-as-errors", &warnings_as_errors,
            "Treat warnings as errors"),
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_STRING,
            .action = APARSE_ACTION_APPEND,
            .longopt = "--define",
            .metavar = "KEY=VALUE",
            .out_val = &definitions,
            .size = sizeof(char *),
            .nargs = APARSE_NARGS(APARSE_NARGS_EXACT, 1),
            .help = "Add a build definition (repeatable)"
        },
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_STRING,
            .action = APARSE_ACTION_APPEND,
            .longopt = "--include",
            .metavar = "DIR",
            .out_val = &include_dirs,
            .size = sizeof(char *),
            .nargs = APARSE_NARGS(APARSE_NARGS_EXACT, 1),
            .help = "Add an include directory (repeatable)"
        },
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_STRING,
            .action = APARSE_ACTION_STORE,
            .longopt = "--feature",
            .metavar = "NAME",
            .out_val = &features,
            .size = sizeof(char *),
            .nargs = APARSE_NARGS(APARSE_NARGS_MINIMUM, 1),
            .help = "Enable one or more features"
        }
    };

    /* ---------------------------------------------------------------------
     * clean arguments
     * --------------------------------------------------------------------- */

    const aparse_argument_t clean_args[] = {
        APARSE_EZ_FLAG(NULL, "--all", &clean_all,
                "Remove all generated files"),
        APARSE_EZ_FLAG(NULL, "--dry-run", &clean_dry_run,
                "Show what would be removed")
    };

    /* ---------------------------------------------------------------------
     * network arguments and nested commands
     * --------------------------------------------------------------------- */

    const aparse_argument_t network_list_args[] = {
        APARSE_EZ_FLAG("-a", "--all", &network_all,
                "Include inactive interfaces"),
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_STRING,
            .action = APARSE_ACTION_STORE,
            .longopt = "--format",
            .metavar = "FORMAT",
            .out_val = &network_format,
            .size = APARSE_STRING_UNBOUNDED,
            .nargs = APARSE_NARGS(APARSE_NARGS_EXACT, 1),
            .default_val = APARSE_PTR(char *, "table"),
            .verify = {
                .verify_fn = verify_string_choice,
                .userdata = (void *)&network_format_choices
            },
            .help = "Output format"
        },
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_UINT,
            .action = APARSE_ACTION_STORE,
            .longopt = "--limit",
            .metavar = "N",
            .out_val = &network_limit,
            .size = sizeof(network_limit),
            .nargs = APARSE_NARGS(APARSE_NARGS_EXACT, 1),
            .help = "Maximum number of results (0 means unlimited)"
        }
    };

    const aparse_argument_t network_add_args[] = {
        APARSE_EZ_POSITIONAL_STRING("NAME", &interface_name_add,
                "Interface name"),
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_STRING,
            .action = APARSE_ACTION_STORE,
            .longopt = "--address",
            .metavar = "IP",
            .out_val = &address,
            .size = APARSE_STRING_UNBOUNDED,
            .nargs = APARSE_NARGS(APARSE_NARGS_EXACT, 1),
            .help = "IP address"
            /* argparse required=True is not represented in the current API. */
        },
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_UINT,
            .action = APARSE_ACTION_STORE,
            .longopt = "--prefix",
            .metavar = "BITS",
            .out_val = &prefix,
            .size = sizeof(prefix),
            .nargs = APARSE_NARGS(APARSE_NARGS_EXACT, 1),
            .default_val = APARSE_PTR(uint32_t, 24),
            .help = "Network prefix length"
        },
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_STRING,
            .action = APARSE_ACTION_STORE,
            .longopt = "--gateway",
            .metavar = "IP",
            .out_val = &gateway,
            .size = APARSE_STRING_UNBOUNDED,
            .nargs = APARSE_NARGS(APARSE_NARGS_EXACT, 1),
            .help = "Default gateway"
        },
        APARSE_EZ_FLAG(NULL, "--enabled", &enabled,
                "Enable the interface immediately")
    };

    const aparse_argument_t network_remove_args[] = {
        APARSE_EZ_POSITIONAL_STRING("NAME", &interface_name_remove,
                "Interface name"),
        APARSE_EZ_FLAG("-f", "--force", &force,
                "Force removal")
    };

    const aparse_argument_t network_commands[] = {
        APARSE_EZ_COMMAND("list", "List network interfaces", network_list_args),
        APARSE_EZ_COMMAND("add", "Add a network interface", network_add_args),
        APARSE_EZ_COMMAND("remove", "Remove a network interface", network_remove_args)
    };

    /* ---------------------------------------------------------------------
     * service arguments and nested commands
     * --------------------------------------------------------------------- */

    const aparse_argument_t service_list_args[] = {
        APARSE_EZ_FLAG(NULL, "--running", &service_running,
                "Show only running services"),
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_STRING,
            .action = APARSE_ACTION_STORE,
            .longopt = "--format",
            .metavar = "FORMAT",
            .out_val = &service_list_format,
            .size = APARSE_STRING_UNBOUNDED,
            .nargs = APARSE_NARGS(APARSE_NARGS_EXACT, 1),
            .default_val = APARSE_PTR(char *, "table"),
            .verify = {
                .verify_fn = verify_string_choice,
                .userdata = (void *)&service_format_choices
            },
            .help = "Output format"
        }
    };

    const aparse_argument_t service_start_args[] = {
        APARSE_EZ_POSITIONAL_STRING("NAME", &service_name_start,
                "Service name"),
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_DOUBLE,
            .action = APARSE_ACTION_STORE,
            .longopt = "--timeout",
            .metavar = "SECONDS",
            .out_val = &start_timeout,
            .size = sizeof(start_timeout),
            .nargs = APARSE_NARGS(APARSE_NARGS_EXACT, 1),
            .default_val = APARSE_PTR(double, 30.0),
            .help = "Startup timeout"
        },
        APARSE_EZ_FLAG(NULL, "--wait", &wait,
                "Wait until the service is ready")
    };

    const aparse_argument_t service_stop_args[] = {
        APARSE_EZ_POSITIONAL_STRING("NAME", &service_name_stop,
                "Service name"),
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_DOUBLE,
            .action = APARSE_ACTION_STORE,
            .longopt = "--timeout",
            .metavar = "SECONDS",
            .out_val = &stop_timeout,
            .size = sizeof(stop_timeout),
            .nargs = APARSE_NARGS(APARSE_NARGS_EXACT, 1),
            .default_val = APARSE_PTR(double, 30.0),
            .help = "Shutdown timeout"
        }
    };

    const aparse_argument_t service_commands[] = {
        APARSE_EZ_COMMAND("list", "List services", service_list_args),
        APARSE_EZ_COMMAND("start", "Start a service", service_start_args),
        APARSE_EZ_COMMAND("stop", "Stop a service", service_stop_args)
    };

    /* ---------------------------------------------------------------------
     * Root command tree
     * --------------------------------------------------------------------- */

    const aparse_argument_t commands[] = {
        APARSE_EZ_COMMAND("build", "Build the project", build_args),
        APARSE_EZ_COMMAND("clean", "Clean build artifacts", clean_args),
        APARSE_EZ_COMMAND("network", "Manage network configuration", network_commands),
        APARSE_EZ_COMMAND("service", "Manage system services", service_commands)
    };

    const aparse_argument_t args[] = {
        APARSE_EZ_COUNT_UINT("-v", "--verbose", &verbosity,
                "Increase verbosity"),
        APARSE_EZ_FLAG("-q", "--quiet", &quiet,
                "Suppress output"),
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_STRING,
            .action = APARSE_ACTION_STORE,
            .longopt = "--config",
            .metavar = "FILE",
            .out_val = &config_file,
            .size = APARSE_STRING_UNBOUNDED,
            .nargs = APARSE_NARGS(APARSE_NARGS_EXACT, 1),
            .help = "Configuration file"
        },
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_STRING,
            .action = APARSE_ACTION_STORE,
            .longopt = "--color",
            .metavar = "WHEN",
            .out_val = &color,
            .size = APARSE_STRING_UNBOUNDED,
            .nargs = APARSE_NARGS(APARSE_NARGS_EXACT, 1),
            .default_val = APARSE_PTR(char *, "auto"),
            .verify = {
                .verify_fn = verify_string_choice,
                .userdata = (void *)&color_choices
            },
            .help = "Color output policy"
        },
        /* argparse add_argument_group("Root options"): named help groups
         * are not currently represented by aparse_argument_t.
         */
        APARSE_EZ_FLAG(NULL, "--debug", &debug,
                "Enable debug mode"),
        APARSE_EZ_FLAG(NULL, "--release", &release,
                "Enable release mode"),
        {
            .kind = APARSE_ARGUMENT_COMMAND,
            .longopt = "command",
            .metavar = "COMMAND",
            .children = commands,
            .children_count = ARRSZ(commands)
            /* required=True subparsers are represented by the command
             * container / missing-command validation in the parser. */
        }
    };

    aparse_error_t rc;
    aparse_result_t result = {0};
    aparse_config_t config = aparse_default_config;
    const aparse_subcommand_node_t *selected;

    /* argparse action="version". The library reports VERSION; the app prints. */
    config.version.shortopt = NULL;
    config.version.longopt = "--version";

    /* argparse options not currently exposed by the aparse API:
     * - allow_abbrev: no configuration switch (this example expects exact matches).
     * - HelpFormatter / ArgumentDefaultsHelpFormatter / RawDescriptionHelpFormatter:
     *   no formatter-class setting. Descriptions are printed manually below.
     * - add_argument_group: no named help-group field.
     * - mutually_exclusive_group: --debug and --release are independent flags.
     */

    rc = aparse_parse_from_argv(
        argc, argv,
        args, ARRSZ(args),
        &config, &result);

    if(rc == APARSE_ERR_USER && result.user_error == APARSE_UERR_HELP)
    {
        aparse_print_usage(&result);
        fputc('\n', stderr);
        print_help_description(&result, args, build_args);
        fputc('\n', stderr);
        aparse_print_help(&result);
    }
    else if(rc == APARSE_ERR_USER && result.user_error == APARSE_UERR_VERSION)
    {
        puts("aparse-demo 1.0.0");
    }
    else if(rc != APARSE_ERR_OK)
    {
        if(rc == APARSE_ERR_USER)
            aparse_print_error(&result);
        else
            fprintf(stderr, "error: %s\n", aparse_strerror(rc));
    }
    else
    {
        printf("Parsed arguments:\n");
        printf("  verbose: %u\n", (unsigned int)verbosity);
        printf("  quiet: %s\n", quiet ? "true" : "false");
        printf("  config: %s\n", STR_OR_NULL(config_file));
        printf("  color: %s\n", STR_OR_NULL(color));
        printf("  debug: %s\n", debug ? "true" : "false");
        printf("  release: %s\n", release ? "true" : "false");

        /* argparse mutually_exclusive_group is not represented in the
         * current aparse API. --debug and --release are therefore independent.
         */

        selected = result.subcommand_head;
        if(selected && selected->command == &commands[0])
        {
            printf("  command: build\n");
            printf("  source: %s\n", STR_OR_NULL(source));
            printf("  output: %s\n", STR_OR_NULL(output));
            printf("  jobs: %u\n", (unsigned int)jobs);
            printf("  optimization: %u\n", (unsigned int)optimization);
            printf("  fast_math: %s\n", fast_math ? "true" : "false");
            printf("  warnings_as_errors: %s\n", warnings_as_errors ? "true" : "false");
            printf("  no_color: %s\n", no_color ? "true" : "false");
            print_string_array("  definitions", &definitions);
            print_string_array("  include_dirs", &include_dirs);
            print_string_array("  features", &features);
        }
        else if(selected && selected->command == &commands[1])
        {
            printf("  command: clean\n");
            printf("  all: %s\n", clean_all ? "true" : "false");
            printf("  dry_run: %s\n", clean_dry_run ? "true" : "false");
        }
        else if(selected && selected->command == &commands[2])
        {
            const aparse_subcommand_node_t *sub = selected->next;
            printf("  command: network\n");

            if(sub && sub->command == &network_commands[0])
            {
                printf("  network_command: list\n");
                printf("  all: %s\n", network_all ? "true" : "false");
                printf("  format: %s\n", STR_OR_NULL(network_format));
                printf("  limit: %u\n", (unsigned int)network_limit);
            }
            else if(sub && sub->command == &network_commands[1])
            {
                printf("  network_command: add\n");
                printf("  name: %s\n", STR_OR_NULL(interface_name_add));
                printf("  address: %s\n", STR_OR_NULL(address));
                printf("  prefix: %u\n", (unsigned int)prefix);
                printf("  gateway: %s\n", STR_OR_NULL(gateway));
                printf("  enabled: %s\n", enabled ? "true" : "false");
            }
            else if(sub && sub->command == &network_commands[2])
            {
                printf("  network_command: remove\n");
                printf("  name: %s\n", STR_OR_NULL(interface_name_remove));
                printf("  force: %s\n", force ? "true" : "false");
            }
        }
        else if(selected && selected->command == &commands[3])
        {
            const aparse_subcommand_node_t *sub = selected->next;
            printf("  command: service\n");

            if(sub && sub->command == &service_commands[0])
            {
                printf("  service_command: list\n");
                printf("  running: %s\n", service_running ? "true" : "false");
                printf("  format: %s\n", STR_OR_NULL(service_list_format));
            }
            else if(sub && sub->command == &service_commands[1])
            {
                printf("  service_command: start\n");
                printf("  name: %s\n", STR_OR_NULL(service_name_start));
                printf("  timeout: %.2f\n", start_timeout);
                printf("  wait: %s\n", wait ? "true" : "false");
            }
            else if(sub && sub->command == &service_commands[2])
            {
                printf("  service_command: stop\n");
                printf("  name: %s\n", STR_OR_NULL(service_name_stop));
                printf("  timeout: %.2f\n", stop_timeout);
            }
        }
    }

    aparse_destroy_array(&features, &config);
    aparse_destroy_array(&include_dirs, &config);
    aparse_destroy_array(&definitions, &config);
    aparse_destroy_result(&result, &config);

    return rc != APARSE_ERR_OK ? 1 : 0;
}
