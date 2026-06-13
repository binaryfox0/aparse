/* Priority=2, FileContentStart=28, FileContentEnd=46
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

/**
 * @brief Dynamic array container.
 *
 * Stores a contiguous sequence of fixed-size elements.
 * Elements are copied into an internal buffer and can be
 * accessed by index.
 */
typedef struct aparse_list
{
    /** Pointer to the underlying element storage. */
    void* ptr;

    /** Number of elements currently stored. */
    size_t size;

    /** Total number of elements that can be stored without resizing. */
    size_t capacity;

    /** Size of each element in bytes. */
    size_t itemsz;
} aparse_list;

/**
 * @brief Retrieves an element from the list.
 *
 * @param list Pointer to the list.
 * @param idx Zero-based element index.
 * @param type Element type.
 *
 * @return The element at @p idx cast to @p type.
 *
 * @warning No bounds checking is performed.
 */
#define aparse_list_get(list, idx, type) (*((type*)(list)->ptr + (idx)))

/**
 * @brief Initializes a list.
 *
 * Allocates storage for a dynamic array capable of holding
 * at least @p init_size elements.
 *
 * @param list List to initialize.
 * @param init_size Initial capacity in elements.
 * @param itemsz Size of each element in bytes.
 *
 * @return 1 on success, 0 on failure.
 */
int aparse_list_new(
        aparse_list* list,
        const size_t init_size,
        const size_t itemsz);

/**
 * @brief Resizes the list storage.
 *
 * Changes the list capacity to hold at least @p new_size elements.
 *
 * @param list List to resize.
 * @param new_size New capacity in elements.
 *
 * @return 1 on success, 0 on failure.
 */
int aparse_list_resize(
        aparse_list* list,
        const size_t new_size);

/**
 * @brief Appends an element to the list.
 *
 * Copies the item pointed to by @p data into the list.
 * The list may be automatically resized if additional
 * capacity is required.
 *
 * @param list Destination list.
 * @param data Pointer to the element to append.
 *
 * @return 1 on success, 0 on failure.
 */
int aparse_list_add(
        aparse_list* list,
        const void* data);

/**
 * @brief Releases all resources owned by the list.
 *
 * Frees the list's storage and resets its fields.
 *
 * @param list List to free.
 */
void aparse_list_free(aparse_list* list);

#endif
