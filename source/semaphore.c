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
#include <assert.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/scheduler.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"
#include "sanoRTOS/semaphore.h"

/**
 * @brief Function to take/wait for the semaphore. If calling this function from an ISR, the parameter waitTicks
 * should be set to TASK_NO_WAIT.
 * @param pSem  pointer to the semaphore structure
 * @param waitTicks Number of ticks to wait if semaphore is not available
 * @retval `RET_SUCCESS` if semaphore is taken succesfully.
 * @retval `RET_BUSY` if semaphore is not available
 * @retval `RET_TIMEOUT` if timeout occured while waiting for semaphore
 */
int semaphoreTake(semaphoreHandleType *pSem, uint32_t waitTicks)
{
    assert(pSem != NULL);

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

        taskQueueAdd(&pSem->waitQueue, currentTask);

        spinUnlock(&pSem->lock, irqState);

        /* Block current task and give CPU to other tasks while waiting for semaphore*/
        taskBlock(currentTask, WAIT_FOR_SEMAPHORE, waitTicks);

        /*Re-acquire spinlock after being unblocked*/
        irqState = spinLock(&pSem->lock);

        if (currentTask->wakeupReason == SEMAPHORE_TAKEN)
        {
            retCode = RET_SUCCESS;
        }
        else if (currentTask->wakeupReason == WAIT_TIMEOUT)
        {
            /*Wait timed out,remove task from  the waitQueue.*/
            taskQueueRemove(&pSem->waitQueue, currentTask);

            /*Wait timed out*/
            retCode = RET_TIMEOUT;
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

/**
 * @brief Function to give/signal semaphore
 * @param pSem  pointer to the semaphoreHandle struct.
 * @retval `RET_SUCCESS` if semaphore give succesfully.
 * @retval `RET_NOSEM` no semaphore available to give
 */
int semaphoreGive(semaphoreHandleType *pSem)
{
    assert(pSem != NULL);

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
            /*If task was suspended while waiting for Semaphore, skip the task and get another waiting task from the waitQueue.*/
            if (nextTask->status == TASK_STATUS_SUSPENDED)
            {
                goto getNextTask;
            }
            taskSetReady(nextTask, SEMAPHORE_TAKEN);

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
