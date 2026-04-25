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

#ifndef __SANO_RTOS_OBJECT_HELPERS_H
#define __SANO_RTOS_OBJECT_HELPERS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/memSlab.h"

#define OBJECT_FLAG_DYNAMIC 0x01U
#define OBJECT_FLAG_OWN_BUFFER 0x02U

static inline int objectAllocFromSlab(memSlabHandleType *pObjectSlab,
                                      void **ppObject,
                                      size_t objectSize)
{
    int retCode;

    if ((pObjectSlab == NULL) || (ppObject == NULL) || (objectSize == 0U))
    {
        return RET_INVAL;
    }

    retCode = memSlabAlloc(pObjectSlab, ppObject, TASK_NO_WAIT);
    if (retCode == RET_SUCCESS)
    {
        memset(*ppObject, 0, objectSize);
        return RET_SUCCESS;
    }

    return (retCode == RET_BUSY) ? RET_NOMEM : retCode;
}

static inline int objectFreeToSlab(memSlabHandleType *pObjectSlab, void *pObject)
{
    if ((pObjectSlab == NULL) || (pObject == NULL))
    {
        return RET_INVAL;
    }

    return memSlabFree(pObjectSlab, pObject);
}

static inline bool objectIsDynamic(uint8_t flags)
{
    return ((flags & OBJECT_FLAG_DYNAMIC) != 0U);
}

static inline bool objectWaitQueueHasWaiters(taskQueueType *pWaitQueue)
{
    return (pWaitQueue != NULL) && !taskQueueEmpty(pWaitQueue);
}

#endif
