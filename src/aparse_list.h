#ifndef APARSE_LIST_H
#define APARSE_LIST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct aparse_list
{
    uint8_t* ptr;
    size_t size;
    size_t capacity;
    size_t var_size;
} aparse_list;

bool aparse_list_new(aparse_list* list, size_t init_size, size_t var_size);
bool aparse_list_resize(aparse_list* list, size_t new_size);

bool aparse_list_add(aparse_list* list, const void* data);
void* aparse_list_get(const aparse_list* list, const size_t index);

void aparse_list_free(aparse_list* list);

#endif