#include "aparse.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define APARSE_LITTLE_ENDIAN (*(unsigned char *)&(uint16_t){1})
#define APARSE_ALLOC_SIZE 5

#define min(a, b) ((a < b) ? (a) : (b))
#define _strcmp(a, b) ((a) ? ((b) ? (!strcmp((a), (b))) : 0) : 0)

typedef struct aparse_internal
{
    int positional_count;
    int positional_processed;
    struct {
        char** data;
        int size;
        int index;
    } unknown;
    bool is_sublayer;
} aparse_internal;

static inline bool aparse_arg_nend(aparse_arg* arg) {
    return arg->longopt != 0 || arg->shortopt != 0;
}

static inline int aparse_args_positional_count(aparse_arg* args)
{
    int count = 0;
    for(; aparse_arg_nend(args); args++)
        if(args->is_positional)
            count++;
    return count;
}

// Forward declaration
int aparse_parse_private(const int argc, char** argv, aparse_arg* args, int* index, aparse_internal* internal);
// For aparse_arg have is_argument == true
void aparse_process_argument(const char* argv, const aparse_arg* arg);
void aparse_process_parser(const int argc, const char* cargv, char** argv, const aparse_arg* arg, int* index);
// For aparse_arg is subparsers and have proper data_layout & layout_size
char* aparse_compose_data(const aparse_arg* args);
void aparse_free_composed(char* data, aparse_arg* args);
// Failure handling
void aparse_add_unknown(const bool found, const char* program_name, aparse_internal* internal);
int aparse_process_failure(const aparse_internal internal, const char* program_name, aparse_arg* args);
void aparse_required_message(const char* argv, aparse_arg* args);
void aparse_unknown_message(const aparse_internal internal, const char* program_name);

void aparse_parse(const int argc, char** argv, aparse_arg* args)
{
    aparse_internal internal = { aparse_args_positional_count(args), 0, {0, 0, 0}};
    int index = 1;
    aparse_parse_private(argc, argv, args, &index, &internal);
    if(aparse_process_failure(internal, argv[0], args))
        exit(EXIT_FAILURE);
}

// --------------------------------------- PRIVATE ---------------------------------------
int aparse_parse_private(const int argc, char** argv, aparse_arg* args, int* index, aparse_internal* internal)
{
    // Prevent out-of-bound process
    if(*index >= argc) 
        return -1;
    int found = 0;
    const char* cargv = argv[*index];
    (*index)++;
    for(aparse_arg* ptr = args; aparse_arg_nend(ptr); ptr++) {
        if(ptr->is_positional)
            found = !ptr->processed;
        else
            found = _strcmp(ptr->shortopt, cargv) || _strcmp(ptr->longopt, cargv);
        if(found) {
            if(ptr->is_positional) {
                ptr->processed = 1;
                internal->positional_processed++;
                if(ptr->is_argument) {
                    aparse_process_argument(cargv, ptr);
                } else {
                    aparse_process_parser(argc, cargv, argv, ptr, index);
                }
            } else {
                fprintf(stderr, "aparse: error: sincerly applogized, we didn't add support for it yet.\n");
            }
            break;
        }
    }
    aparse_add_unknown(found, cargv, internal);

    // No more thing to process (at least for this layer), this for handling optional of the layer > 1
    if(internal->is_sublayer && !found) {
        *index -= 1;
        return -1;
    }

    if(*index < argc)
        aparse_parse_private(argc, argv, args, index, internal);
    return 0;
}

void aparse_process_argument(const char* argv, const aparse_arg *arg) {
    if (!arg->ptr) return;

    if (arg->is_number) {
        uint64_t num = arg->have_sign ? (int64_t)strtoll(argv, NULL, 10) : strtoull(argv, NULL, 10);
        memcpy(arg->ptr, &num, min(arg->size, 8));

        // Directly manipulating the sign bit (MSB)
        if (arg->have_sign) {
            uint8_t* b = (uint8_t*)arg->ptr + (APARSE_LITTLE_ENDIAN ? 3 : 0);
            *b = (*b & ~0x80) | (((int64_t)num < 0) ? 0x80 : 0);
        }
    } else {
        size_t len = strlen(argv) + 1;

        if (arg->auto_allocation) {
            char** dst = (char**)arg->ptr;
            char* tmp = realloc(*dst, len); // leak origin
            if (!tmp) return;
            *dst = tmp;
            memcpy(*dst, argv, len);
        } else {
            size_t n = min(arg->size - 1, len);
            memcpy(arg->ptr, argv, n);
            ((char*)arg->ptr)[n] = '\0';
        }
    }
}

void aparse_process_parser(const int argc, const char* cargv, char** argv, const aparse_arg* arg, int* index)
{
    // Find subcommand for parser
    aparse_arg* ptr2 = arg->subargs;
    int found2;
    for(; aparse_arg_nend(ptr2); ptr2++) {
            if(found2 = !strcmp(cargv, ptr2->longopt))
                break;
        }
    if(!found2) {
        fprintf(stderr, "%s: error: invalid choice: '%s' (choose from ", argv[0], argv);
        for(aparse_arg* ptr2 = arg->subargs; aparse_arg_nend(ptr2); ptr2++) {
            fprintf(stderr, "%s%s", ptr2->longopt, aparse_arg_nend(ptr2 + 1) ? ", " : "");
        }
        fprintf(stderr, ")\n");
        exit(EXIT_FAILURE);
    }
    // Found it!, compose struct for it to handler process.
    char* data = aparse_compose_data(ptr2);
    aparse_internal internal2 = { aparse_args_positional_count(ptr2->subargs), 0, {0}, 1};
    aparse_parse_private(argc, argv, ptr2->subargs, index, &internal2);
    if(aparse_process_failure(internal2, argv[0], ptr2->subargs)) {
        aparse_free_composed(data, ptr2->subargs);
        exit(EXIT_FAILURE);
    }
    if(ptr2->handler)
        ptr2->handler(data);
    aparse_free_composed(data, ptr2->subargs);            
}

// Use to create a bytes-like structure to hold data before parsing then pass into handler
char* aparse_compose_data(const aparse_arg* args)
{
    if(!args->subargs || !args->handler)
        return 0;
    aparse_arg* real = args->subargs;
    // Query size
    int index = 0, count = 0;
    for(; aparse_arg_nend(real); real++)
    {
        if(count == args->layout_size)
            break;
        if(!real->is_argument || !real->is_positional)
            continue;
        index += real->auto_allocation ? sizeof(void*) : real->size;
    }
    char* buffer = (char*)malloc(index);
    if(!buffer) return 0;
    memset(buffer, 0, index);
    // Determine the maximum size it can have
    count = 0;
    index = 0;
    real = args->subargs;
    for(; aparse_arg_nend(real); real++)
    {
        if(count == args->layout_size)
            break;
        if(!real->is_argument || !real->is_positional)
            continue;
        // Obviously that is_number only have two state: true/false so dont need if
        real->ptr = &buffer[index];
        index += real->auto_allocation ? sizeof(void*) : real->size;
        count++;
    }
    return buffer;
}

void aparse_free_composed(char* data, aparse_arg* args)
{
    if(!args)
        return;
    for(aparse_arg* real = args; aparse_arg_nend(real); real++) {
        if(real->auto_allocation && real->processed) {
            char** dst = (char**)(real->ptr);
            free(*dst);
        }
    }
    free(data);
}

void aparse_add_unknown(const bool found, const char* argv, aparse_internal* internal)
{
    for(int i = 0; i < 1 && !found; i++) {
        // Trigger allocation
        if(internal->unknown.index >= internal->unknown.size)
        {
            char** tmp = realloc(internal->unknown.data, sizeof(char*) * (internal->unknown.size + APARSE_ALLOC_SIZE));
            if(!tmp) break;
            internal->unknown.data = tmp;
            internal->unknown.size += APARSE_ALLOC_SIZE;
        }
        int size = strlen(argv) + 1;
        if(!(internal->unknown.data[internal->unknown.index] = malloc(size)))
            break;
        memcpy(internal->unknown.data[internal->unknown.index], argv, size);
        internal->unknown.index++;
    }
}

// 0 no error, 1 error (just for cleaning up)
int aparse_process_failure(const aparse_internal internal, const char* program_name, aparse_arg* args)
{
    int error = 0;
    if (internal.positional_count != internal.positional_processed) {
        aparse_required_message(program_name, args);
        error = 1;
    }
    if (internal.unknown.size > 0) {
        aparse_unknown_message(internal, program_name);
        error = 1;
    }
    return error;
}


void aparse_required_message(const char* argv, aparse_arg* args)
{
    fprintf(stderr, "%s: error: the following arguments are required: ", argv);

    int printed = 0;
    for (int i = 0; aparse_arg_nend(args); args++) {
        if (args->is_positional && !args->processed) {
            if (printed > 0)
                fprintf(stderr, ", ");
            fprintf(stderr, "%s", args->longopt);
            printed++;
        }
    }

    fprintf(stderr, "\n");
}

void aparse_unknown_message(const aparse_internal internal, const char* program_name)
{
    fprintf(stderr, "%s: error: unrecognized arguments: ", program_name);
    int printed = 0;
    for (int i = 0; i < internal.unknown.index; i++) {
        if (printed > 0)
            fprintf(stderr, ", ");
        fprintf(stderr, "%s", internal.unknown.data[i]);
        free(internal.unknown.data[i]);
        printed++;
    }
    fprintf(stderr, "\n");
    // Cleanup
    free(internal.unknown.data);
}