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
#include "retCodes.h"
#include "task/task.h"
#include "scheduler/scheduler.h"
#include "taskQueue/taskQueue.h"
#include "semaphore.h"

/**
 * @brief Function to take/wait for the semaphore. If calling this function from an ISR, the parameter waitTicks
 * should be set to TASK_NO_WAIT.
 * @param pSem  pointer to the semaphore structure
 * @param waitTicks Number of ticks to wait if semaphore is not available
 * @retval RET_SUCCESS if semaphore is taken succesfully.
 * @retval RET_BUSY if semaphore is not available
 * @retval RET_TIMEOUT if timeout occured while waiting for semaphore
 */
int semaphoreTake(semaphoreHandleType *pSem, uint32_t waitTicks)
{
    assert(pSem != NULL);

    int retCode;

    ENTER_CRITICAL_SECTION();

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
        taskHandleType *currentTask = taskPool.currentTask;

        /*Put current task in semaphore's wait queue*/
        taskQueueAdd(&pSem->waitQueue, currentTask);

        /*Exit from critical section before blocking the task*/
        EXIT_CRITICAL_SECTION();

        /* Block current task and give CPU to other tasks while waiting for semaphore*/
        taskBlock(currentTask, WAIT_FOR_SEMAPHORE, waitTicks);

        /*Re-enter critical section after being unblocked*/
        ENTER_CRITICAL_SECTION();

        if (currentTask->wakeupReason == SEMAPHORE_TAKEN)
        {
            retCode = RET_SUCCESS;
        }
        else
        {
            /*Wait timed out*/
            retCode = RET_TIMEOUT;
        }
    }
    EXIT_CRITICAL_SECTION();

    return retCode;
}

/**
 * @brief Function to give/signal semaphore
 * @param pSem  pointer to the semaphoreHandle struct.
 * @retval RET_SUCCESS if semaphore give succesfully.
 * @retval RET_NOSEM no semaphore available to give
 */
int semaphoreGive(semaphoreHandleType *pSem)
{
    assert(pSem != NULL);

    int retCode;

    bool contextSwitchRequired = false;

    ENTER_CRITICAL_SECTION();

    if (pSem->count != pSem->maxCount)
    {
        /*Get next highest priority task to unblock from the wait Queue*/
        taskHandleType *nextTask = taskQueueGet(&pSem->waitQueue);

        if (nextTask != NULL)
        {
            taskSetReady(nextTask, SEMAPHORE_TAKEN);

            /*Perform context switch if unblocked task has equal or
             *higher priority[lower priority value] than that of current task */
            if (nextTask->priority <= taskPool.currentTask->priority)
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

    EXIT_CRITICAL_SECTION();

    if (contextSwitchRequired)
    {
        taskYield();
    }

    return retCode;
}