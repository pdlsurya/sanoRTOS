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
#include "osConfig.h"
#include "retCodes.h"
#include "task/task.h"
#include "scheduler/scheduler.h"
#include "taskQueue/taskQueue.h"
#include "mutex.h"

/**
 * @brief Lock/acquire the mutex
 *
 * @param pMutex Pointer to the mutex structure
 * @param waitTicks Number of ticks to wait if mutex is not available
 * @retval RET_SUCCESS if mutex locked successfully
 * @retval RET_BUSY  if mutex not available
 * @retval RET_TIMEOUT if timeout occured while waiting for mutex
 */
int mutexLock(mutexHandleType *pMutex, uint32_t waitTicks)
{
    assert(pMutex != NULL);

    taskHandleType *currentTask = taskPool.currentTask;
#if MUTEX_USE_PRIORITY_INHERITANCE
    /* Priority inheritance*/
    if (pMutex->ownerTask && currentTask->priority < pMutex->ownerTask->priority)
    {
        /* Save owner task's default priority if not saved before */
        if (pMutex->ownerDefaultPriority == -1)
        {
            pMutex->ownerDefaultPriority = pMutex->ownerTask->priority;
        }
        pMutex->ownerTask->priority = currentTask->priority;
    }
#endif
    /* Check if mutex is free and no owner has been assigned. If so, lock mutex immediately.*/
    if (!pMutex->locked)
    {
        pMutex->locked = true;
        pMutex->ownerTask = currentTask;
        return RET_SUCCESS;
    }

    else if (waitTicks == TASK_NO_WAIT && pMutex->locked)
    {
        return RET_BUSY;
    }

    else
    {
        /* Add the tasking waiting on mutex to the wait queue*/
        taskQueueAdd(&pMutex->waitQueue, currentTask);

        /* Block current task and give CPU to other tasks while waiting for mutex*/
        taskBlock(currentTask, WAIT_FOR_MUTEX, waitTicks);

        if (currentTask->wakeupReason == MUTEX_LOCKED && pMutex->ownerTask == currentTask)
        {
            return RET_SUCCESS;
        }

        return RET_TIMEOUT;
    }
}

/**
 * @brief Unlock/Release mutex
 * @param pMutex Pointer to the mutex structure
 * @retval RET_SUCCESS if mutex unlocked successfully
 * @retval RET_NOTOWNER if current owner doesnot owns the mutex
 * @retval RET_NOTLOCKED if mutex was not previously locked
 */
int mutexUnlock(mutexHandleType *pMutex)
{

    assert(pMutex != NULL);

    taskHandleType *currentTask = taskPool.currentTask;

    /*Unlocking the mutex is possible only if current task owns it*/

    if (pMutex->ownerTask == currentTask)
    {
        if (pMutex->locked)
        {

#if MUTEX_USE_PRIORITY_INHERITANCE
            /* Assign owner task its default priority if priority inheritance was perforemd while locking the mutex*/
            if (pMutex->ownerDefaultPriority != -1)
            {
                pMutex->ownerTask->priority = pMutex->ownerDefaultPriority;

                /* Reset owner defalult priority of mutex*/
                pMutex->ownerDefaultPriority = -1;
            }
#endif
            /* select next owner of the mutex*/
            taskHandleType *nextOwner = taskQueueGet(&pMutex->waitQueue);

            pMutex->ownerTask = nextOwner;

            if (nextOwner != NULL)
            {
                taskSetReady(nextOwner, MUTEX_LOCKED);
            }
            else
            {
                pMutex->locked = false;
            }

            return RET_SUCCESS;
        }

        return RET_NOTLOCKED;
    }

    return RET_NOTOWNER;
}
