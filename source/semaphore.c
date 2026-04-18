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

    bool irqState = spinLock(&pSem->lock);

retry:
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

        /*Put current task in semaphore's wait queue*/

        retCode = taskQueueAdd(&pSem->waitQueue, currentTask);
        if (retCode != RET_SUCCESS)
        {
            spinUnlock(&pSem->lock, irqState);
            return retCode;
        }

        spinUnlock(&pSem->lock, irqState);

        /* Block current task and give CPU to other tasks while waiting for semaphore*/
        retCode = taskBlock(currentTask, WAIT_FOR_SEMAPHORE, waitTicks);
        if (retCode != RET_SUCCESS)
        {
            irqState = spinLock(&pSem->lock);
            (void)taskQueueRemove(&pSem->waitQueue, currentTask);
            spinUnlock(&pSem->lock, irqState);
            return retCode;
        }

        /*Re-acquire spinlock after being unblocked*/
        irqState = spinLock(&pSem->lock);

        if (currentTask->wakeupReason == SEMAPHORE_TAKEN)
        {
            retCode = RET_SUCCESS;
        }
        else if (currentTask->wakeupReason == WAIT_TIMEOUT)
        {
            /*Wait timed out,remove task from  the waitQueue.*/
            retCode = taskQueueRemove(&pSem->waitQueue, currentTask);
            if ((retCode == RET_SUCCESS) || (retCode == RET_NOTASK))
            {
                retCode = RET_TIMEOUT;
            }

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
        /*Get next highest priority task to unblock from the wait Queue*/
    getNextTask:
        nextTask = TASK_GET_FROM_WAIT_QUEUE(&pSem->waitQueue);

        if (nextTask != NULL)
        {
            /*Skip stale tasks that are no longer blocked waiting for this semaphore.*/
            if ((nextTask->status != TASK_STATUS_BLOCKED) ||
                (nextTask->blockedReason != WAIT_FOR_SEMAPHORE))
            {
                goto getNextTask;
            }
            retCode = taskSetReady(nextTask, SEMAPHORE_TAKEN);
            if (retCode != RET_SUCCESS)
            {
                spinUnlock(&pSem->lock, irqState);
                return retCode;
            }

            taskHandleType *currentTask = taskGetCurrent();

            /*Perform context switch if unblocked task has equal or
             *higher priority[lower priority value] than that of current task */
            if (nextTask->priority <= currentTask->priority)
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
