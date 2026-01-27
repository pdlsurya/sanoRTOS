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

#include <stdint.h>
#include "sanoRTOS/memSlab.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/scheduler.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"

static int memSlabInitLocked(memSlabHandleType *pMemSlab)
{
    if (pMemSlab == NULL)
    {
        return RET_INVAL;
    }

    if (pMemSlab->initialized)
    {
        return RET_SUCCESS;
    }

    if ((pMemSlab->buffer == NULL) ||
        (pMemSlab->blockSize < sizeof(void *)) ||
        ((pMemSlab->blockSize % sizeof(void *)) != 0U) ||
        (pMemSlab->numBlocks == 0U))
    {
        return RET_INVAL;
    }

    uint8_t *pCurrentBlock = pMemSlab->buffer;
    for (uint32_t blockIndex = 0; blockIndex < pMemSlab->numBlocks; blockIndex++)
    {
        uint8_t *pNextBlock = NULL;
        if ((blockIndex + 1U) < pMemSlab->numBlocks)
        {
            pNextBlock = pCurrentBlock + pMemSlab->blockSize;
        }

        *(void **)pCurrentBlock = pNextBlock;
        pCurrentBlock += pMemSlab->blockSize;
    }

    pMemSlab->freeList = pMemSlab->buffer;
    pMemSlab->freeBlocks = pMemSlab->numBlocks;
    pMemSlab->initialized = true;

    return RET_SUCCESS;
}

static bool memSlabBlockIsValid(memSlabHandleType *pMemSlab, void *pBlock)
{
    if ((pMemSlab == NULL) || (pBlock == NULL) || (pMemSlab->buffer == NULL))
    {
        return false;
    }

    uintptr_t blockAddr = (uintptr_t)pBlock;
    uintptr_t slabStart = (uintptr_t)pMemSlab->buffer;
    uintptr_t slabEnd = slabStart + ((uintptr_t)pMemSlab->blockSize * (uintptr_t)pMemSlab->numBlocks);

    if ((blockAddr < slabStart) || (blockAddr >= slabEnd))
    {
        return false;
    }

    return (((blockAddr - slabStart) % pMemSlab->blockSize) == 0U);
}

static bool memSlabBlockIsFreeLocked(memSlabHandleType *pMemSlab, void *pBlock)
{
    if ((pMemSlab == NULL) || (pBlock == NULL))
    {
        return false;
    }

    void *pCurrentBlock = pMemSlab->freeList;
    while (pCurrentBlock != NULL)
    {
        if (pCurrentBlock == pBlock)
        {
            return true;
        }

        pCurrentBlock = *(void **)pCurrentBlock;
    }

    return false;
}

static int memSlabWakeWaitingTask(memSlabHandleType *pMemSlab, bool *pContextSwitchRequired)
{
    if ((pMemSlab == NULL) || (pContextSwitchRequired == NULL))
    {
        return RET_INVAL;
    }

    taskHandleType *pTask = NULL;

getNextWaitingTask:
    pTask = TASK_GET_FROM_WAIT_QUEUE(&pMemSlab->waitQueue);
    if (pTask != NULL)
    {
        if ((pTask->status != TASK_STATUS_BLOCKED) ||
            (pTask->blockedReason != WAIT_FOR_MEM_SLAB))
        {
            goto getNextWaitingTask;
        }

        int retCode = taskSetReady(pTask, MEM_SLAB_AVAILABLE);
        if (retCode != RET_SUCCESS)
        {
            return retCode;
        }

        if (taskCanPreemptCurrentCore(pTask))
        {
            *pContextSwitchRequired = true;
        }
    }

    return RET_SUCCESS;
}

int memSlabAlloc(memSlabHandleType *pMemSlab, void **ppBlock, uint32_t waitTicks)
{
    if ((pMemSlab == NULL) || (ppBlock == NULL))
    {
        return RET_INVAL;
    }
    if (portIsInISRContext() && (waitTicks != TASK_NO_WAIT))
    {
        return RET_INVAL;
    }

    int retCode;
    bool irqState;
    *ppBlock = NULL;

retry:
    irqState = spinLock(&pMemSlab->lock);

    retCode = memSlabInitLocked(pMemSlab);
    if (retCode != RET_SUCCESS)
    {
        spinUnlock(&pMemSlab->lock, irqState);
        return retCode;
    }

    if (pMemSlab->freeList != NULL)
    {
        *ppBlock = pMemSlab->freeList;
        pMemSlab->freeList = *(void **)pMemSlab->freeList;
        pMemSlab->freeBlocks--;
        retCode = RET_SUCCESS;
    }
    else if (waitTicks == TASK_NO_WAIT)
    {
        retCode = RET_BUSY;
    }
    else
    {
        taskHandleType *currentTask = taskGetCurrent();

        retCode = taskQueueAdd(&pMemSlab->waitQueue, currentTask);
        if (retCode != RET_SUCCESS)
        {
            spinUnlock(&pMemSlab->lock, irqState);
            return retCode;
        }

        spinUnlock(&pMemSlab->lock, irqState);

        retCode = taskBlock(currentTask, WAIT_FOR_MEM_SLAB, waitTicks);
        if (retCode != RET_SUCCESS)
        {
            irqState = spinLock(&pMemSlab->lock);
            (void)taskQueueRemove(&pMemSlab->waitQueue, currentTask);
            spinUnlock(&pMemSlab->lock, irqState);
            return retCode;
        }

        if (currentTask->wakeupReason == MEM_SLAB_AVAILABLE)
        {
            goto retry;
        }
        else if (currentTask->wakeupReason == WAIT_TIMEOUT)
        {
            irqState = spinLock(&pMemSlab->lock);
            retCode = taskQueueRemove(&pMemSlab->waitQueue, currentTask);
            spinUnlock(&pMemSlab->lock, irqState);

            if ((retCode == RET_SUCCESS) || (retCode == RET_NOTASK))
            {
                retCode = RET_TIMEOUT;
            }
        }
        else
        {
            goto retry;
        }

        return retCode;
    }

    spinUnlock(&pMemSlab->lock, irqState);

    return retCode;
}

int memSlabFree(memSlabHandleType *pMemSlab, void *pBlock)
{
    if ((pMemSlab == NULL) || (pBlock == NULL))
    {
        return RET_INVAL;
    }

    bool irqState = spinLock(&pMemSlab->lock);

    int retCode = memSlabInitLocked(pMemSlab);
    if (retCode != RET_SUCCESS)
    {
        spinUnlock(&pMemSlab->lock, irqState);
        return retCode;
    }

    if (!memSlabBlockIsValid(pMemSlab, pBlock))
    {
        spinUnlock(&pMemSlab->lock, irqState);
        return RET_INVAL;
    }
    if (memSlabBlockIsFreeLocked(pMemSlab, pBlock))
    {
        spinUnlock(&pMemSlab->lock, irqState);
        return RET_INVAL;
    }
    if (pMemSlab->freeBlocks >= pMemSlab->numBlocks)
    {
        spinUnlock(&pMemSlab->lock, irqState);
        return RET_INVAL;
    }

    *(void **)pBlock = pMemSlab->freeList;
    pMemSlab->freeList = pBlock;
    pMemSlab->freeBlocks++;

    bool contextSwitchRequired = false;
    retCode = memSlabWakeWaitingTask(pMemSlab, &contextSwitchRequired);

    spinUnlock(&pMemSlab->lock, irqState);

    if (retCode != RET_SUCCESS)
    {
        return retCode;
    }

    if (contextSwitchRequired)
    {
        taskYield();
    }

    return RET_SUCCESS;
}
