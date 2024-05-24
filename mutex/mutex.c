/**
 * @file mutex.c
 * @author Surya Poudel
 * @brief Mutex lock implementation
 * @version 0.1
 * @date 2024-04-09
 *
 * @copyright Copyright (c) 2024
 *
 */
#include <stdlib.h>
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
 * @retval RET_INVAL if invalid arguments passed
 */
int mutexLock(mutexHandleType *pMutex, uint32_t waitTicks)
{
    if (pMutex != NULL)
    {
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

        else if (waitTicks > 0)
        {
            /* Add the tasking waiting on mutex to the wait queue*/
            taskQueueAdd(&pMutex->waitQueue, currentTask);

            /* Block current task and give CPU to other tasks while waiting for mutex*/
            taskBlock(currentTask, WAIT_FOR_MUTEX, waitTicks);

            if (currentTask->wakeupReason == MUTEX_LOCKED && pMutex->ownerTask == currentTask)
            {
                return RET_SUCCESS;
            }
            else
            {
                return RET_TIMEOUT;
            }
        }
    }
    return RET_INVAL;
}

/**
 * @brief Unlock/Release mutex
 * @param pMutex Pointer to the mutex structure
 * @retval RET_SUCCESS if mutex unlocked successfully
 * @retval RET_NOTOWNER if current owner doesnot owns the mutex
 * @retval RET_INVAL on Invalid operation or invalid argument passed
 */
int mutexUnlock(mutexHandleType *pMutex)
{
    taskHandleType *currentTask = taskPool.currentTask;

    /*Unlocking the mutex is possible only if current task owns it*/
    if (pMutex != NULL)
    {
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
        }
        else
        {
            return RET_NOTOWNER;
        }
    }

    return RET_INVAL;
}
