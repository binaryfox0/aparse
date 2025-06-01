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