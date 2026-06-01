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
#include "taskInternal.h"
#include "objectHelpers.h"

MEM_SLAB_DEFINE(dynamicCondVarObjectSlab,
                sizeof(condVarHandleType),
                CONFIG_DYNAMIC_COND_VAR_SLAB_BLOCKS);

static int condVarObjectAlloc(condVarHandleType **ppCondVar)
{
    return objectAllocFromSlab(&dynamicCondVarObjectSlab,
                               (void **)ppCondVar,
                               sizeof(condVarHandleType));
}

static void condVarObjectFree(condVarHandleType *pCondVar)
{
    (void)objectFreeToSlab(&dynamicCondVarObjectSlab, pCondVar);
}

static int condVarSetup(condVarHandleType *pCondVar,
                        mutexHandleType *pMutex, uint8_t flags)
{
    if ((pCondVar == NULL) || (pMutex == NULL))
    {
        return RET_INVAL;
    }

    memset(pCondVar, 0, sizeof(condVarHandleType));
    pCondVar->flags = flags;
    pCondVar->waitQueue.pLock = &pCondVar->lock;
    pCondVar->pMutex = pMutex;

    return RET_SUCCESS;
}

int condVarWait(condVarHandleType *pCondVar, uint32_t waitTicks)
{
    if ((pCondVar == NULL) || (pCondVar->pMutex == NULL))
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
    retCode = taskBlockOnWaitQueue(&pCondVar->waitQueue,
                                   WAIT_FOR_COND_VAR,
                                   waitTicks,
                                   irqState);
    if (retCode != RET_SUCCESS)
    {
        return retCode;
    }

    /*Task has been woken up either due to wait timeout or by another task by signalling the condtion variable.*/
    if (currentTask->wakeupReason == COND_VAR_SIGNALLED)
    {
        retCode = RET_SUCCESS;
    }
    else if (currentTask->wakeupReason == WAIT_TIMEOUT)
    {
        retCode = RET_TIMEOUT;
    }
    /*Task might have been suspended while waiting on condition variable and later resumed.
      In this case, retry waiting on condition variable again */
    else
    {
        irqState = spinLock(&pCondVar->lock);
        goto wait;
    }

    /*Re-acquire previously released mutex*/
    if ((retCode == RET_SUCCESS) || (retCode == RET_TIMEOUT))
    {
        int lockRetCode = mutexLock(pCondVar->pMutex, TASK_FOREVER_WAIT);
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

    int retCode;

    bool contextSwitchRequired = false;
    taskHandleType *nextSignalTask = NULL;

    bool irqState = spinLock(&pCondVar->lock);

    nextSignalTask = waitQueuePop(&pCondVar->waitQueue);

    if (nextSignalTask != NULL)
    {
        retCode = taskSetReady(nextSignalTask, COND_VAR_SIGNALLED);
        if (retCode != RET_SUCCESS)
        {
            spinUnlock(&pCondVar->lock, irqState);
            return retCode;
        }

        /*Perform context switch if unblocked task has equal or
         *higher priority[lower priority value] than that of current task */
        if (taskCanPreemptCurrentCore(nextSignalTask))
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

    int retCode;

    bool irqState = spinLock(&pCondVar->lock);

    if (!taskQueueEmpty(&pCondVar->waitQueue))
    {
        taskHandleType *pTask = NULL;

        while ((pTask = waitQueuePop(&pCondVar->waitQueue)) != NULL)
        {
            retCode = taskSetReady(pTask, COND_VAR_SIGNALLED);
            if (retCode != RET_SUCCESS)
            {
                spinUnlock(&pCondVar->lock, irqState);
                return retCode;
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

int condVarCreate(condVarHandleType **ppCondVar, mutexHandleType *pMutex)
{
    condVarHandleType *pCondVar = NULL;
    int retCode;

    if (ppCondVar == NULL)
    {
        return RET_INVAL;
    }

    retCode = condVarObjectAlloc(&pCondVar);
    if (retCode != RET_SUCCESS)
    {
        return retCode;
    }

    retCode = condVarSetup(pCondVar, pMutex, OBJECT_FLAG_DYNAMIC);
    if (retCode != RET_SUCCESS)
    {
        condVarObjectFree(pCondVar);
        return retCode;
    }

    *ppCondVar = pCondVar;

    return RET_SUCCESS;
}

int condVarDelete(condVarHandleType *pCondVar)
{
    bool irqState;

    if ((pCondVar == NULL) || !objectIsDynamic(pCondVar->flags))
    {
        return RET_INVAL;
    }

    irqState = spinLock(&pCondVar->lock);
    if (objectWaitQueueHasWaiters(&pCondVar->waitQueue))
    {
        spinUnlock(&pCondVar->lock, irqState);
        return RET_BUSY;
    }

    spinUnlock(&pCondVar->lock, irqState);
    condVarObjectFree(pCondVar);

    return RET_SUCCESS;
}
