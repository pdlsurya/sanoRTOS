/**
 * @file semaphore.c
 * @author Surya Poudel
 * @brief
 * @version 0.1
 * @date 2024-04-09
 *
 * @copyright Copyright (c) 2024
 *
 */
#include <stdlib.h>
#include <assert.h>
#include "retCodes.h"
#include "task/task.h"
#include "scheduler/scheduler.h"
#include "taskQueue/taskQueue.h"
#include "semaphore.h"

/**
 * @brief Function to take/wait for the semaphore
 *
 * @param pSem  pointer to the semaphore structure
 * @param waitTicks Number of ticks to wait if semaphore is not available
 * @retval RET_SUCCESS if semaphore is taken succesfully.
 * @retval RET_BUSY if semaphore is not available
 * @retval RET_TIMEOUT if timeout occured while waiting for semaphore
 */
int semaphoreTake(semaphoreHandleType *pSem, uint32_t waitTicks)
{
    assert(pSem != NULL);

    if (pSem->count != 0)
    {
        pSem->count--;
        return RET_SUCCESS;
    }

    else if (waitTicks == TASK_NO_WAIT)
    {
        return RET_BUSY;
    }
    else
    {
        taskHandleType *currentTask = taskPool.currentTask;

        /*Put current task in semaphore's wait queue*/
        taskQueueAdd(&pSem->waitQueue, currentTask);

        /* Block current task and give CPU to other tasks while waiting for semaphore*/
        taskBlock(currentTask, WAIT_FOR_SEMAPHORE, waitTicks);

        if (currentTask->wakeupReason == SEMAPHORE_TAKEN)
        {
            return RET_SUCCESS;
        }

        return RET_TIMEOUT;
    }
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

    if (pSem->count != pSem->maxCount)
    {
        /*Get next task to signal from the wait Queue*/
        taskHandleType *nextTask = taskQueueGet(&pSem->waitQueue);

        if (nextTask != NULL)
        {
            taskSetReady(nextTask, SEMAPHORE_TAKEN);
        }
        else
        {
            pSem->count++;
        }

        return RET_SUCCESS;
    }

    return RET_NOSEM;
}