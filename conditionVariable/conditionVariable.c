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
#include <stdlib.h>
#include <assert.h>
#include "osConfig.h"
#include "retCodes.h"
#include "task/task.h"
#include "mutex/mutex.h"
#include "scheduler/scheduler.h"
#include "taskQueue/taskQueue.h"
#include "conditionVariable.h"

/**
 * @brief Wait on condition variable. Since waiting on a condition variable internally uses
 * a mutex, this function cannot be called from an ISR.
 * @param pCondVar Pointer to condVarHandle struct
 * @param waitTicks Number of ticks to wait until timeout
 * @retval RET_SUCCESS(0) if wait succeeded
 * @retval RET_TIMEOUT if timeout occured while waiting
 */
int condVarWait(condVarHandleType *pCondVar, uint32_t waitTicks)
{
    assert(pCondVar != NULL);
    assert(pCondVar->pMutex != NULL);

    int retCode;

    /* Unlock previously acquired mutex;*/
    mutexUnlock(pCondVar->pMutex);

    taskHandleType *currentTask = taskPool.currentTask;

wait:
    taskQueueAdd(&pCondVar->waitQueue, currentTask);

    /* Block current task and give CPU to other tasks while waiting on condition variable*/
    taskBlock(currentTask, WAIT_FOR_COND_VAR, waitTicks);

    /*Task has been woken up either due to wait timeout or by another task by signalling the condtion variable.*/
    if (currentTask->wakeupReason == COND_VAR_SIGNALLED)
    {
        retCode = RET_SUCCESS;
    }
    else if (currentTask->wakeupReason == WAIT_TIMEOUT)
    {
        /*Wait timed out,remove task from  the waitQueue.*/
        taskQueueRemove(&pCondVar->waitQueue, currentTask);

        retCode = RET_TIMEOUT;
    }
    /*Task might have been suspended while waiting on condition variable and later resumed.
      In this case, retry waiting on condition variable again */
    else
    {
        goto wait;
    }

    /*Re-acquire previously released mutex*/
    mutexLock(pCondVar->pMutex, TASK_MAX_WAIT);

    return retCode;
}

/**
 * @brief Signal a task waiting on conditional variable. This function cannot be
 * called from an ISR.
 * @param pCondVar Pointer to condVarHandle struct
 * @retval RET_SUCCESS if signal succeeded,
 * @retval RET_NOTASK if no tasks available to signal
 */
int condVarSignal(condVarHandleType *pCondVar)
{
    assert(pCondVar != NULL);

    taskHandleType *nextSignalTask = NULL;

    /*Get next highest priority waiting task to unblock*/
getNextSignalTask:
    nextSignalTask = taskQueueGet(&pCondVar->waitQueue);

    if (nextSignalTask != NULL)
    {
        /*If task was suspended while waiting on condition varibale, skip the task and get another waiting task from the waitQueue*/
        if (nextSignalTask->status == TASK_STATUS_SUSPENDED)
        {
            goto getNextSignalTask;
        }
        taskSetReady(nextSignalTask, COND_VAR_SIGNALLED);

        /*Perform context switch if unblocked task has equal or
         *higher priority[lower priority value] than that of current task */
        if (nextSignalTask->priority <= taskPool.currentTask->priority)
        {
            taskYield();
        }
        return RET_SUCCESS;
    }

    return RET_NOTASK;
}

/**
 * @brief Signal all the waiting tasks waiting on conditional variable. This function
 * cannot be called from an ISR.
 * @param pCondVar Pointer to condVarHandle struct
 * @retval RET_SUCCESS if broadcast succeeded,
 * @retval RET_NOTASK if not tasks available to broadcast
 */
int condVarBroadcast(condVarHandleType *pCondVar)
{
    assert(pCondVar != NULL);

    if (!taskQueueEmpty(&pCondVar->waitQueue))
    {
        taskHandleType *pTask = NULL;

        while ((pTask = taskQueueGet(&pCondVar->waitQueue)))
        {
            if (pTask->status != TASK_STATUS_SUSPENDED)
            {
                taskSetReady(pTask, COND_VAR_SIGNALLED);
            }
        }

        return RET_SUCCESS;
    }
    return RET_NOTASK;
}
