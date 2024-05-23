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
#include "task/task.h"
#include "scheduler/scheduler.h"
#include "taskQueue/taskQueue.h"
#include "semaphore.h"

/**
 * @brief Function to take/wait for the semaphore
 *
 * @param pSem  pointer to the semaphore structure
 * @param waitTicks Number of ticks to wait if semaphore is not available
 * @return true, if semaphore is taken succesfully.
 * @return false, if semaphore could not be taken
 */
bool semaphoreTake(semaphoreHandleType *pSem, uint32_t waitTicks)
{
    if (pSem)
    {
        if (pSem->count != 0)
        {
            pSem->count--;
            return true;
        }

        else if (waitTicks == TASK_NO_WAIT)
            return false;
        else
        {
            taskHandleType *currentTask = taskPool.currentTask;

            /*Put current task in semaphore's wait queue*/
            taskQueueAdd(&pSem->waitQueue, currentTask);

            /* Block current task and give CPU to other tasks while waiting for semaphore*/
            taskBlock(currentTask, WAIT_FOR_SEMAPHORE, waitTicks);

            if (currentTask->wakeupReason == SEMAPHORE_AVAILABLE && pSem->count != 0)
            {
                pSem->count--;
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief Function to give/signal semaphore
 * @param pSem  pointer to the semaphoreHandle struct.
 * @return true if semaphore given successfully
 * @return  false if semaphore could not be given
 */
bool semaphoreGive(semaphoreHandleType *pSem)
{
    if (pSem && pSem->count != pSem->maxCount)
    {
        pSem->count++;

        /*Get next task to signal from the wait Queue*/
        taskHandleType *nextTask = taskQueueGet(&pSem->waitQueue);

        if (nextTask)
            taskSetReady(nextTask, SEMAPHORE_AVAILABLE);

        return true;
    }

    return false;
}