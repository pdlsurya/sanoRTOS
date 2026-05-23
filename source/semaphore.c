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
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/scheduler.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"
#include "sanoRTOS/semaphore.h"
#include "objectHelpers.h"
#include "taskInternal.h"

MEM_SLAB_DEFINE(dynamicSemaphoreObjectSlab,
                sizeof(semaphoreHandleType),
                CONFIG_DYNAMIC_SEMAPHORE_SLAB_BLOCKS);

static int semaphoreObjectAlloc(semaphoreHandleType **ppSem)
{
    return objectAllocFromSlab(&dynamicSemaphoreObjectSlab,
                               (void **)ppSem,
                               sizeof(semaphoreHandleType));
}

static void semaphoreObjectFree(semaphoreHandleType *pSem)
{
    (void)objectFreeToSlab(&dynamicSemaphoreObjectSlab, pSem);
}

static int semaphoreSetup(semaphoreHandleType *pSem,
                          uint8_t initialCount, uint8_t maxCount, uint8_t flags)
{
    if ((pSem == NULL) || (maxCount == 0U) || (initialCount > maxCount))
    {
        return RET_INVAL;
    }

    memset(pSem, 0, sizeof(semaphoreHandleType));
    pSem->flags = flags;
    pSem->waitQueue.pLock = &pSem->lock;
    pSem->count = initialCount;
    pSem->maxCount = maxCount;

    return RET_SUCCESS;
}

int semaphoreTake(semaphoreHandleType *pSem, uint32_t waitTicks)
{
    if (pSem == NULL)
    {
        return RET_INVAL;
    }
    if (portIsInISRContext() && (waitTicks != TASK_NO_WAIT))
    {
        return RET_INVAL;
    }

    int retCode;
    bool irqState;

retry:
    irqState = spinLock(&pSem->lock);

    if (pSem->count != 0)
    {
        pSem->count--;

        retCode = RET_SUCCESS;
    }

    else if (waitTicks == TASK_NO_WAIT)
    {

        retCode = RET_BUSY;
    }
    else
    {
        taskHandleType *currentTask = taskGetCurrent();

        retCode = taskBlockOnWaitQueue(&pSem->waitQueue,
                                       WAIT_FOR_SEMAPHORE,
                                       waitTicks,
                                       irqState);
        if (retCode != RET_SUCCESS)
        {
            return retCode;
        }

        if (currentTask->wakeupReason == SEMAPHORE_TAKEN)
        {
            return RET_SUCCESS;
        }
        else if (currentTask->wakeupReason == WAIT_TIMEOUT)
        {
            return RET_TIMEOUT;
        }
        /*Task might have been suspended while waiting for semaphore and later resumed.
          In this case, retry taking the semaphore again */
        else
        {
            goto retry;
        }
    }
    spinUnlock(&pSem->lock, irqState);

    return retCode;
}

int semaphoreGive(semaphoreHandleType *pSem)
{
    if (pSem == NULL)
    {
        return RET_INVAL;
    }

    int retCode;

    bool contextSwitchRequired = false;

    taskHandleType *nextTask = NULL;

    bool irqState = spinLock(&pSem->lock);

    if (pSem->count != pSem->maxCount)
    {
        nextTask = waitQueuePop(&pSem->waitQueue);

        if (nextTask != NULL)
        {
            retCode = taskSetReady(nextTask, SEMAPHORE_TAKEN);
            if (retCode != RET_SUCCESS)
            {
                spinUnlock(&pSem->lock, irqState);
                return retCode;
            }

            /*Perform context switch if unblocked task has equal or
             *higher priority[lower priority value] than that of current task */
            if (taskCanPreemptCurrentCore(nextTask))
            {
                contextSwitchRequired = true;
            }
        }
        else
        {
            pSem->count++;
        }

        retCode = RET_SUCCESS;
    }
    else
    {
        retCode = RET_NOSEM;
    }

    spinUnlock(&pSem->lock, irqState);

    if (contextSwitchRequired)
    {
        taskYield();
    }

    return retCode;
}

int semaphoreCreate(semaphoreHandleType **ppSem, uint8_t initialCount, uint8_t maxCount)
{
    semaphoreHandleType *pSem = NULL;
    int retCode;

    if (ppSem == NULL)
    {
        return RET_INVAL;
    }

    retCode = semaphoreObjectAlloc(&pSem);
    if (retCode != RET_SUCCESS)
    {
        return retCode;
    }

    retCode = semaphoreSetup(pSem, initialCount, maxCount, OBJECT_FLAG_DYNAMIC);
    if (retCode != RET_SUCCESS)
    {
        semaphoreObjectFree(pSem);
        return retCode;
    }

    *ppSem = pSem;

    return RET_SUCCESS;
}

int semaphoreDelete(semaphoreHandleType *pSem)
{
    bool irqState;

    if ((pSem == NULL) || !objectIsDynamic(pSem->flags))
    {
        return RET_INVAL;
    }

    irqState = spinLock(&pSem->lock);
    if (objectWaitQueueHasWaiters(&pSem->waitQueue))
    {
        spinUnlock(&pSem->lock, irqState);
        return RET_BUSY;
    }

    spinUnlock(&pSem->lock, irqState);
    semaphoreObjectFree(pSem);

    return RET_SUCCESS;
}
