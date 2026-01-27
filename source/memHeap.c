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
#include "sanoRTOS/memHeap.h"
#include "sanoRTOS/spinLock.h"

static atomic_t lock;

void *memHeapAlloc(size_t size)
{
    void *ptr = NULL;

    bool irqState = spinLock(&lock);

    ptr = malloc(size);

    spinUnlock(&lock, irqState);

    return ptr;
}

void memHeapFree(void *ptr)
{
    bool irqState = spinLock(&lock);

    free(ptr);

    spinUnlock(&lock, irqState);
}

void *memHeapRealloc(void *ptr, size_t size)
{
    void *new_ptr = NULL;

    bool irqState = spinLock(&lock);

    new_ptr = realloc(ptr, size);

    spinUnlock(&lock, irqState);

    return new_ptr;
}
