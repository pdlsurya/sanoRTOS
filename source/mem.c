/*
 * MIT License
 *
 * Copyright (c) 2024 Surya Poudel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "sanoRTOS/mem.h"
#include "sanoRTOS/spinLock.h"

static atomic_t lock;

/**
 * @brief Allocate a  memory block of specified size in bytes
 *
 * @param size Size of memory to be allocated in bytes
 * @retval Address of the allocated memory block
 */
void *memAlloc(size_t size)
{
    void *ptr;
    bool irqFlag = spinLock(&lock);

    ptr = malloc(size);

    spinUnlock(&lock, irqFlag);
    return ptr;
}

/**
 * @brief Deallocate the memory block
 *
 * @param ptr Pointer to the  memory block
 */
void memFree(void *ptr)
{
    bool irqFlag = spinLock(&lock);

    free(ptr);

    spinUnlock(&lock, irqFlag);
}

/**
 * @brief Re-allocate the  memory block with given size
 *
 * @param ptr Pointer to the memory block to be reallocated
 * @param size New size of the memory block
 * @retval Address of new memeory block
 */
void *memRealloc(void *ptr, size_t size)
{
    void *new_ptr;
    bool irqFlag = spinLock(&lock);

    new_ptr = realloc(ptr, size);

    spinUnlock(&lock, irqFlag);
    return new_ptr;
}
