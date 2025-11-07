/* Priority=1, FileContentStart=25
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

#include "aparse_list.h"

#include <stdlib.h>
#include <string.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

bool aparse_list_new(aparse_list* list, size_t init_size, size_t var_size) {
    if(!var_size)
        return false;
    list->var_size = var_size;
    list->size = 0;
    list->ptr = NULL;
    list->capacity = 0;
    return init_size > 0 ? aparse_list_resize(list, init_size) : false;
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
