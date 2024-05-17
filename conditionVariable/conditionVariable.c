/**
 * @file conditionVariable.c
 * @author Surya Poudel
 * @brief Condition variable implementation
 * @version 0.1
 * @date 2024-05-10
 *
 * @copyright Copyright (c) 2024
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include "osConfig.h"
#include "task/task.h"
#include "mutex/mutex.h"
#include "scheduler/scheduler.h"
#include "utils/utils.h"
#include "conditionVariable.h"
#include "assert.h"

/**
 * @brief Initialize condition variable
 *
 * @param pCondVar  Pointer to the Condition variable handle struct
 * @param pMutex   Pointer to the Mutex handle struct to be used by the condition variable.
 */
void condVarInit(condVarHandleType *pCondVar, mutexHandleType *pMutex)
{
    assert(pMutex != NULL);
    // Initialize mutex
    mutexInit(pMutex);

    pCondVar->pMutex = pMutex;

    waitQueueInit(&pCondVar->waitQueue);
}

/**
 * @brief Wait on condition variable
 *
 * @param pCondVar Pointer to condVarHandle struct
 * @param waitTicks Number of ticks to wait until timeout
 * @return true if wait succeeded
 * @return false if wait failed due to invalid mutex parameter or timeout
 */
bool condVarWait(condVarHandleType *pCondVar, uint32_t waitTicks)
{
    if (!pCondVar->pMutex)
        return false;

    /* Unlock previously acquired mutex;*/
    mutexUnlock(pCondVar->pMutex);

    taskHandleType *currentTask = taskPool.tasks[taskPool.currentTaskId];

    waitQueuePut(&pCondVar->waitQueue, currentTask);

    /* Block current task and give CPU to other tasks while waiting on condition variable*/
    taskBlock(currentTask, WAIT_FOR_COND_VAR, waitTicks);

    /*Task has been woken up either due to timeout or by another task by signalling the condtion variable. Re-acquire previously
    release mutex and return */
    mutexLock(pCondVar->pMutex, TASK_MAX_WAIT);

    /* Return false if wait timed out.*/
    if (currentTask->wakeupReason == WAIT_TIMEOUT)
        return false;

    return true;
}

/**
 * @brief Signal a task waiting on conditional variable.
 *
 * @param pCondVar Pointer to condVarHandle struct
 * @return true if signal succeeded,
 * @return false if signal failed
 */
bool condVarSignal(condVarHandleType *pCondVar)
{
    taskHandleType *nextSignalTask = waitQueueGet(&pCondVar->waitQueue);
    if (nextSignalTask)
    {
        taskSetReady(nextSignalTask, COND_VAR_SIGNALLED);
        return true;
    }

    return false;
}

/**
 * @brief Signal all the waiting tasks waiting on conditional variable
 *
 * @param pCondVar Pointer to condVarHandle struct
 * @return true if broadcast succeeded,
 * @return false if broadcast failed
 */
bool condVarBroadcast(condVarHandleType *pCondVar)
{
    if (pCondVar->waitQueue.count > 0)
    {
        for (int i = 0; i < pCondVar->waitQueue.count; i++)
        {
            if (pCondVar->waitQueue.waitingTasks[i])
            {
                /* Signal the task only if it is not suspended*/
                if (pCondVar->waitQueue.waitingTasks[i]->status != TASK_STATUS_SUSPENDED)
                {
                    taskSetReady(pCondVar->waitQueue.waitingTasks[i], COND_VAR_SIGNALLED);
                    pCondVar->waitQueue.waitingTasks[i] = NULL;
                }
            }
        }
        pCondVar->waitQueue.count = 0;
        return true;
    }
    return false;
}