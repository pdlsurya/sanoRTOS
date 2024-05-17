/**
 * @file mutex.c
 * @author Surya Poudel
 * @brief Mutex implementation
 * @version 0.1
 * @date 2024-04-09
 *
 * @copyright Copyright (c) 2024
 *
 */
#include <stdlib.h>
#include "task/task.h"
#include "scheduler/scheduler.h"
#include "utils/utils.h"
#include "mutex.h"
#include "osConfig.h"

/**
 * @brief Select next owner of the mutex
 *
 * @param pMutex Pointer to mutexHandle struct.
 */
static inline void selectNextOwner(mutexHandleType *pMutex)
{
    taskHandleType *nextOwner = waitQueueGet(&pMutex->waitQueue);

    pMutex->ownerTask = nextOwner;

    if (nextOwner)
        taskSetReady(nextOwner, MUTEX_AVAILABLE);
}

/**
 * @brief Initialize mutex
 *
 * @param pMutex Pointer to mutexHandle struct
 * @return true on Success
 * @return false on Fail
 */
bool mutexInit(mutexHandleType *pMutex)
{
    if (pMutex)
    {
        pMutex->locked = false;
        pMutex->ownerTask = NULL;
        pMutex->ownerDefaultPriority = -1;
        waitQueueInit(&pMutex->waitQueue);

        return true;
    }
    return false;
}

/**
 * @brief Lock/acquire the mutex
 *
 * @param pMutex Pointer to the mutex structure
 * @param waitTicks Number of ticks to wait if mutex is not available
 * @return true, if mutex is locked successfully
 * @return false, if mutex could not be locked
 */
bool mutexLock(mutexHandleType *pMutex, uint32_t waitTicks)
{
    if (pMutex)
    {
        taskHandleType *currentTask = taskPool.tasks[taskPool.currentTaskId];
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
        if (!pMutex->locked && pMutex->ownerTask == NULL)
        {
            pMutex->locked = true;
            pMutex->ownerTask = currentTask;
            return true;
        }

        else if (waitTicks == TASK_NO_WAIT && pMutex->locked)
            return false;

        else if (waitTicks > 0)
        {
            /* Add the tasking waiting on mutex to the wait queue*/
            waitQueuePut(&pMutex->waitQueue, currentTask);

            /* Block current task and give CPU to other tasks while waiting for mutex*/
            taskBlock(currentTask, WAIT_FOR_MUTEX, waitTicks);

            if (currentTask->wakeupReason == MUTEX_AVAILABLE && pMutex->ownerTask == currentTask && !pMutex->locked)
            {
                pMutex->locked = true;
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief Unlock/Release mutex
 * @param pMutex Pointer to the mutex structure
 * @return true if mutex unlocked successfully
 * @return false if mutex could not be unlocked
 */
bool mutexUnlock(mutexHandleType *pMutex)
{
    taskHandleType *currentTask = taskPool.tasks[taskPool.currentTaskId];

    /*Unlocking the mutex is possible only if current task owns it*/
    if (pMutex != NULL && pMutex->ownerTask == currentTask)
    {
        if (pMutex->locked)
        {
            pMutex->locked = false;

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
            selectNextOwner(pMutex);

            return true;
        }
    }

    return false;
}
