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

#include <assert.h>
#include <stdio.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/config.h"
#include "sanoRTOS/spinLock.h"
#include "sanoRTOS/scheduler.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/timer.h"
#include "sanoRTOS/log.h"

LOG_MODULE_DEFINE(task);

taskPoolType taskPool = {0};

/*Currently scheduled task*/
taskHandleType *currentTask[PORT_CORE_COUNT];

/*Next task to be scheduled*/
taskHandleType *nextTask[PORT_CORE_COUNT];

static atomic_t lock;

/**
 * @brief Function to execute when task returns
 *
 */
void taskExitFunction()
{

    while (1)
        ;
}

/**
 * @brief Change task's status to ready
 * @param pTask Pointer to the taskHandle struct.
 */
void taskSetReady(taskHandleType *pTask, wakeupReasonType wakeupReason)
{
    assert(pTask != NULL);

    bool irqState = spinLock(&lock);

    if (pTask->status == TASK_STATUS_BLOCKED)
    {
        taskQueueType *pBlockedQueue = getBlockedQueue();

        /* Remove  task from the queue of blocked tasks*/
        taskQueueRemove(pBlockedQueue, pTask);
    }
    pTask->status = TASK_STATUS_READY;
    pTask->blockedReason = BLOCK_REASON_NONE;
    pTask->wakeupReason = wakeupReason;
    pTask->remainingSleepTicks = 0;

    taskQueueType *pReadyQueue = getReadyQueue();

    /* Add task to queue of ready tasks*/
    taskQueueAdd(pReadyQueue, pTask);

    spinUnlock(&lock, irqState);
}

/**
 * @brief Block task with the specified blocking reason and number to ticks to block the task for.
 *
 * @param pTask Pointer to taskHandle struct.
 * @param blockReason Block reason
 * @param ticks Number to ticks to block the task for.
 */
void taskBlock(taskHandleType *pTask, blockedReasonType blockedReason, uint32_t ticks)
{
    assert(pTask != NULL);

    bool irqState = spinLock(&lock);

    pTask->remainingSleepTicks = ticks;
    pTask->status = TASK_STATUS_BLOCKED;
    pTask->blockedReason = blockedReason;
    pTask->wakeupReason = WAKEUP_REASON_NONE;

    taskQueueType *pBlockedQueue = getBlockedQueue();

    // Add task to queue of blocked tasks. We dont need to sort tasks in blockedQueue
    taskQueueAddToFront(pBlockedQueue, pTask);

    spinUnlock(&lock, irqState);

    // Give CPU to other tasks
    taskYield();
}

/**
 * @brief Suspend the specified task
 *
 * @param pTask Pointer to taskHandle struct.
 */
void taskSuspend(taskHandleType *pTask)
{
    assert(pTask != NULL);

    bool irqState = spinLock(&lock);

    /* If task status is ready, remove it from the readyQueue*/
    if (pTask->status == TASK_STATUS_READY)
    {
        taskQueueType *pReadyQueue = getReadyQueue();
        taskQueueRemove(pReadyQueue, pTask);
    }
    /*If task status is blocked, remove it from the blockedQueue*/
    else if (pTask->status == TASK_STATUS_BLOCKED)
    {
        taskQueueType *pBlockedQueue = getBlockedQueue();
        taskQueueRemove(pBlockedQueue, pTask);
    }

    pTask->remainingSleepTicks = 0;
    pTask->status = TASK_STATUS_SUSPENDED;
    pTask->blockedReason = BLOCK_REASON_NONE;
    pTask->wakeupReason = WAKEUP_REASON_NONE;

    spinUnlock(&lock, irqState);

    /*If self suspended, give CPU to other tasks*/
    if (pTask == taskGetCurrent())
    {
        taskYield();
    }
}

/**
 * @brief Resume task from suspended state
 *
 * @param pTask Pointer to taskHandle struct
 * @return `RET_SUCCESS` if task resumed succesfully
 * @return `RET_NOTSUSPENDED` if task is not suspended
 */
int taskResume(taskHandleType *pTask)
{
    assert(pTask != NULL);

    if (pTask->status == TASK_STATUS_SUSPENDED)
    {
        taskSetReady(pTask, RESUME);
        return RET_SUCCESS;
    }

    return RET_NOTSUSPENDED;
}

/**
 * @brief Store pointer to the taskHandle struct to the queue of ready tasks. Calling this
 * function from main does not start execution of the task if Scheduler is not started.To start executeion of task, osStartScheduler must be
 * called from  main after calling taskStart. If this function is called from other running tasks, execution happens based on priority of the task.
 *
 * @param pTask Pointer to taskHandle struct
 */
void taskStart(taskHandleType *pTask)
{
    assert(pTask != NULL);

    bool irqState = spinLock(&lock);

    taskQueueType *pReadyQueue = getReadyQueue();

    taskQueueAdd(pReadyQueue, pTask);

    spinUnlock(&lock, irqState);
}

/**
 * @brief Checks if the current task has experienced a stack overflow.
 *
 * This function is not meant to be called by the user. It is used internally by the
 * scheduler to check for stack overflows. If a stack overflow is detected, the program
 * halts in an infinite loop.
 */
void taskCheckStackOverflow(void)
{
    taskHandleType *pCurrentTask = taskGetCurrent();

    if (pCurrentTask->stackPointer <= (uint32_t)(pCurrentTask->stack + STACK_GUARD_WORDS))
    {
        LOG_ERROR("%s stack overflow at address: %p", pCurrentTask->taskName, pCurrentTask->stackPointer);

        for (;;)
            ;
    }
}
