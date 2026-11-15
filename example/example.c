#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <aparse/aparse.h>

#define ARRSZ(x) (sizeof((x)) / sizeof((x)[0]))

static aparse_verify_result_t verify_optimization(
        void *userdata,
        const aparse_argument_t *argument,
        const void *value)
{
    uint32_t optimization = *(const uint32_t*)value;
    if(optimization > 3)
        return "expected optimization level from 0 to 3";
    return NULL;
}

int main(int argc, char **argv)
{
    const char *source = NULL;
    const char *output = NULL;
    uint32_t jobs = 0;
    uint32_t optimization = 0;
    bool fast_math = false;
    bool warning_as_errors = false;
    aparse_array_t definitions = {0};
    aparse_array_t features = {0};
    
    const aparse_argument_t build_args[] = 
    {
        {
            .kind = APARSE_ARGUMENT_POSITIONAL,
            .type = APARSE_TYPE_STRING,
            .longopt = "source",
            .metavar = "SOURCE",
            .out_val = &source,
            .size = APARSE_STRING_UNBOUNDED,
            .help = "Source directory"
        },
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_STRING,
            .shortopt = "-o",
            .longopt = "--output",
            .metavar = "DIR",
            .out_val = &output,
            .size = APARSE_STRING_UNBOUNDED,
            .help = "Output directory"
        },
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_UINT,
            .shortopt = "-j",
            .longopt = "--jobs",
            .metavar = "N",
            .size = sizeof(jobs),
            .out_val = &jobs,
            .help = "Number of parallel jobs"
        },
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_UINT,
            .verify = (aparse_verify_t){
                .verify_fn = verify_optimization
            },
            .longopt = "--optimization",
            .metavar = "LEVEL",
            .out_val = &optimization,
            .size = sizeof(optimization),
            .help = "Optimization level (0-3)"
        },
        {
            .kind = APARSE_ARGUMENT_FLAG,
            .longopt = "--fast-math",
            .out_val = &fast_math,
            .help = "Enable fast floating-point math"
        },
        {
            .kind = APARSE_ARGUMENT_FLAG,
            .longopt = "--warnings-as-errors",
            .out_val = &warning_as_errors,
            .help = "Treat warnings as errors"
        },
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_STRING,
            .longopt = "--define",
            .out_val = &definitions,
            .action = APARSE_ACTION_APPEND,
            .help = "Add a build definition (repetable)"
        },
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .type = APARSE_TYPE_STRING,
            .longopt = "--feature",
            .out_val = &features,
            .nargs = APARSE_NARGS(APARSE_NARGS_MINIMUM, 1),
            .help = "Enable one or more features",
            .metavar = "NAME"
        }
    };

    const aparse_argument_t commands[] = 
    {
        {
            .longopt = "build",
            .children = build_args,
            .children_count = ARRSZ(build_args),
            .help = "Build the project"
        },
        {
            .longopt = "clean",
            .children = build_args,
            .children_count = ARRSZ(build_args),
            .help = "Clean build artifacts"
        },
        {
            .longopt = "network",
            .children = build_args,
            .children_count = ARRSZ(build_args),
            .help = "Manage network configuration"
        },
        {
            .longopt = "service",
            .children = build_args,
            .children_count = ARRSZ(build_args),
            .help = "Manage system services"
        },
    };

    const aparse_argument_t args[] =
    {
        {
            .kind = APARSE_ARGUMENT_FLAG,
            .longopt = "--verbose",
            .shortopt = "-v",
            .help = "Increase verbosity"
        },
        {
            .kind = APARSE_ARGUMENT_FLAG,
            .longopt = "--quiet",
            .shortopt = "-q",
            .help = "Supress output"
        },
        {
            .kind = APARSE_ARGUMENT_OPTION,
            .longopt = "--config",
            .metavar = "FILE",
            .help = "Configuration file"
        },
        {
            .kind = APARSE_ARGUMENT_COMMAND,
            .children = commands,
            .children_count = ARRSZ(commands),
            .longopt = "command",
            .metavar = "COMMAND"
        }
    };
    aparse_error_t rc = APARSE_ERR_OK;
    aparse_result_t res = {0};
    aparse_config_t config = aparse_default_config;

    config.version.shortopt = NULL;

    rc = aparse_parse_from_argv(argc, argv, args, ARRSZ(args), &config, &res);    
    if(rc == APARSE_ERR_USER)
    {
        if(res.user_error == APARSE_UERR_HELP)
        {
            aparse_print_usage(&res);
            fputc('\n', stderr);
            if(res.arg == args)
                fprintf(stderr, "Reference CLI for testing aparse features.\n");
            else if(res.arg == build_args)
                fprintf(stderr, "Build the project using the selected configuration.\n");
            fputc('\n', stderr);
            aparse_print_help(&res);
        }
        else
            aparse_print_error(&res);
    } 
    else if(rc != APARSE_ERR_OK)
    {
        fprintf(stderr, "error: %s\n", aparse_strerror(rc));
    }
    else
    {
        aparse_subcommand_node_t *head = res.subcommand_head;
        while(head)
        {
            if(head->command == build_args)
            {
                printf("Source directory: \"%s\"\n", source);
                printf("Output directory: \"%s\"\n", output);
                printf("Optimization level: %d\n", optimization);
                printf("Fast math: %s\n", fast_math ? "true" : "false");
                printf("Warnings as errors: %s\n", warning_as_errors ? "true" : "false");
                printf("Definitions:\n");
                for(size_t i = 0; i < definitions.size; i++)
                    printf(" - %s\n", ((const char**)definitions.data)[i]);
                printf("Features:\n");
                for(size_t i = 0; i < features.size; i++)
                    printf(" - %s\n", ((const char**)features.data)[i]);

            }

            head = head->next;
        }
    }

    aparse_destroy_array(&features, &config);
    aparse_destroy_array(&definitions, &config);
    aparse_destroy_result(&res, &config);
    return rc != APARSE_ERR_OK ? 1 : 0;
}