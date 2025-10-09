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
#include "sanoRTOS/config.h"
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/mutex.h"
#include "sanoRTOS/scheduler.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/conditionVariable.h"

int condVarWait(condVarHandleType *pCondVar, uint32_t waitTicks)
{
    if ((pCondVar == NULL) || (pCondVar->pMutex == NULL))
    {
        return RET_INVAL;
    }
    if (portIsInISRContext())
    {
        return RET_INVAL;
    }

    int retCode;

    bool irqState = spinLock(&pCondVar->lock);

    /* Unlock previously acquired mutex;*/
    retCode = mutexUnlock(pCondVar->pMutex);
    if (retCode != RET_SUCCESS)
    {
        spinUnlock(&pCondVar->lock, irqState);
        return retCode;
    }

    taskHandleType *currentTask = taskGetCurrent();

wait:
    retCode = taskQueueAdd(&pCondVar->waitQueue, currentTask);
    if (retCode != RET_SUCCESS)
    {
        spinUnlock(&pCondVar->lock, irqState);
        return retCode;
    }

    spinUnlock(&pCondVar->lock, irqState);

    /* Block current task and give CPU to other tasks while waiting on condition variable*/
    retCode = taskBlock(currentTask, WAIT_FOR_COND_VAR, waitTicks);
    if (retCode != RET_SUCCESS)
    {
        irqState = spinLock(&pCondVar->lock);
        (void)taskQueueRemove(&pCondVar->waitQueue, currentTask);
        spinUnlock(&pCondVar->lock, irqState);
        return retCode;
    }

    irqState = spinLock(&pCondVar->lock);

    /*Task has been woken up either due to wait timeout or by another task by signalling the condtion variable.*/
    if (currentTask->wakeupReason == COND_VAR_SIGNALLED)
    {
        retCode = RET_SUCCESS;
    }
    else if (currentTask->wakeupReason == WAIT_TIMEOUT)
    {
        /*Wait timed out,remove task from  the waitQueue.*/
        retCode = taskQueueRemove(&pCondVar->waitQueue, currentTask);
        if ((retCode == RET_SUCCESS) || (retCode == RET_NOTASK))
        {
            retCode = RET_TIMEOUT;
        }
    }
    /*Task might have been suspended while waiting on condition variable and later resumed.
      In this case, retry waiting on condition variable again */
    else
    {
        goto wait;
    }
    spinUnlock(&pCondVar->lock, irqState);

    /*Re-acquire previously released mutex*/
    if ((retCode == RET_SUCCESS) || (retCode == RET_TIMEOUT))
    {
        int lockRetCode = mutexLock(pCondVar->pMutex, TASK_MAX_WAIT);
        if (lockRetCode != RET_SUCCESS)
        {
            return lockRetCode;
        }
    }

    return retCode;
}

int condVarSignal(condVarHandleType *pCondVar)
{
    if (pCondVar == NULL)
    {
        return RET_INVAL;
    }
    if (portIsInISRContext())
    {
        return RET_INVAL;
    }

    int retCode;

    bool contextSwitchRequired = false;

    taskHandleType *nextSignalTask = NULL;

    bool irqState = spinLock(&pCondVar->lock);

    /*Get next highest priority waiting task to unblock*/
getNextSignalTask:

    nextSignalTask = TASK_GET_FROM_WAIT_QUEUE(&pCondVar->waitQueue);

    if (nextSignalTask != NULL)
    {
        /*Skip stale tasks that are no longer blocked waiting on this condition variable.*/
        if ((nextSignalTask->status != TASK_STATUS_BLOCKED) ||
            (nextSignalTask->blockedReason != WAIT_FOR_COND_VAR))
        {
            goto getNextSignalTask;
        }
        retCode = taskSetReady(nextSignalTask, COND_VAR_SIGNALLED);
        if (retCode != RET_SUCCESS)
        {
            spinUnlock(&pCondVar->lock, irqState);
            return retCode;
        }

        taskHandleType *currentTask = taskGetCurrent();

        /*Perform context switch if unblocked task has equal or
         *higher priority[lower priority value] than that of current task */
        if (nextSignalTask->priority <= currentTask->priority)
        {
            contextSwitchRequired = true;
        }
        retCode = RET_SUCCESS;
    }
    else
    {
        retCode = RET_NOTASK;
    }

    spinUnlock(&pCondVar->lock, irqState);

    if (contextSwitchRequired)
    {
        taskYield();
    }

    return retCode;
}

int condVarBroadcast(condVarHandleType *pCondVar)
{
    if (pCondVar == NULL)
    {
        return RET_INVAL;
    }
    if (portIsInISRContext())
    {
        return RET_INVAL;
    }

    int retCode;

    bool irqState = spinLock(&pCondVar->lock);

    if (!taskQueueEmpty(&pCondVar->waitQueue))
    {
        taskHandleType *pTask = NULL;

        while ((pTask = TASK_GET_FROM_WAIT_QUEUE(&pCondVar->waitQueue)) != NULL)
        {
            if ((pTask->status == TASK_STATUS_BLOCKED) &&
                (pTask->blockedReason == WAIT_FOR_COND_VAR))
            {
                retCode = taskSetReady(pTask, COND_VAR_SIGNALLED);
                if (retCode != RET_SUCCESS)
                {
                    spinUnlock(&pCondVar->lock, irqState);
                    return retCode;
                }
            }
        }

        retCode = RET_SUCCESS;
    }
    else
    {
        retCode = RET_NOTASK;
    }
    spinUnlock(&pCondVar->lock, irqState);

    return retCode;
}
