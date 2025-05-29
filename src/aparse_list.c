#include "aparse_list.h"

#include <stdlib.h>
#include <string.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

bool aparse_list_new(aparse_list* list, size_t init_size, size_t var_size) {
    list->var_size = var_size;
    list->size = 0;
    list->ptr = NULL;
    list->capacity = 0;
    return aparse_list_resize(list, init_size);
}

bool aparse_list_resize(aparse_list* list, size_t new_size) {
    if (!list->var_size)
        return true;

    if (new_size) {
        void* tmp = realloc(list->ptr, new_size * list->var_size);
        if (!tmp) return true;
        list->ptr = tmp;
        list->capacity = new_size;
        list->size = min(list->size, new_size);
    } else {
        free(list->ptr);
        list->ptr = NULL;
        list->capacity = 0;
        list->size = 0;
    }
    return false;
}

bool aparse_list_add(aparse_list* list, const void* data) {
    if (!list->var_size)
        return true;

    if (list->size >= list->capacity) {
        size_t new_capacity = list->capacity ? list->capacity * 2 : 4;
        if (aparse_list_resize(list, new_capacity))
            return true;
    }

    memcpy((uint8_t*)list->ptr + list->size * list->var_size, data, list->var_size);
    list->size++;
    return false;
}

void* aparse_list_get(const aparse_list* list, size_t index) {
    if (index >= list->size)
        return NULL;
    return (uint8_t*)list->ptr + index * list->var_size;
}

void aparse_list_free(aparse_list* list) {
    free(list->ptr);
    list->ptr = NULL;
    list->size = 0;
    list->capacity = 0;
    list->var_size = 0;
}
