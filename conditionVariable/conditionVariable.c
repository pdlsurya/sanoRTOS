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
#include "retCodes.h"
#include "task/task.h"
#include "mutex/mutex.h"
#include "scheduler/scheduler.h"
#include "taskQueue/taskQueue.h"
#include "conditionVariable.h"

/**
 * @brief Wait on condition variable
 *
 * @param pCondVar Pointer to condVarHandle struct
 * @param waitTicks Number of ticks to wait until timeout
 * @retval SUCCESS(0) if wait succeeded\n,
 * @retval -EINVAL if mutex is not specified
 * @retval -ETIMEOUT if timeout occured while waiting
 */
int condVarWait(condVarHandleType *pCondVar, uint32_t waitTicks)
{
    if (!pCondVar->pMutex)
        return -EINVAL;

    /* Unlock previously acquired mutex;*/
    mutexUnlock(pCondVar->pMutex);

    taskHandleType *currentTask = taskPool.currentTask;

    taskQueueAdd(&pCondVar->waitQueue, currentTask);

    /* Block current task and give CPU to other tasks while waiting on condition variable*/
    taskBlock(currentTask, WAIT_FOR_COND_VAR, waitTicks);

    /*Task has been woken up either due to timeout or by another task by signalling the condtion variable. Re-acquire previously
    release mutex and return */
    mutexLock(pCondVar->pMutex, TASK_MAX_WAIT);

    /* Return false if wait timed out.*/
    if (currentTask->wakeupReason == WAIT_TIMEOUT)
        return -ETIMEOUT;

    return SUCCESS;
}

/**
 * @brief Signal a task waiting on conditional variable.
 *
 * @param pCondVar Pointer to condVarHandle struct
 * @retval SUCCESS if signal succeeded,
 * @retval -ENOTASK if no tasks available to signal
 */
int condVarSignal(condVarHandleType *pCondVar)
{
    taskHandleType *nextSignalTask = taskQueueGet(&pCondVar->waitQueue);
    if (nextSignalTask)
    {
        taskSetReady(nextSignalTask, COND_VAR_SIGNALLED);
        return SUCCESS;
    }

    return -ENOTASK;
}

/**
 * @brief Signal all the waiting tasks waiting on conditional variable
 *
 * @param pCondVar Pointer to condVarHandle struct
 * @retval SUCCESS if broadcast succeeded,
 * @retval -ENOTASK if not tasks available to broadcast
 */
int condVarBroadcast(condVarHandleType *pCondVar)
{
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

        return SUCCESS;
    }
    return -ENOTASK;
}