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
#include "retCodes.h"
#include "osConfig.h"
#include "scheduler/scheduler.h"
#include "taskQueue/taskQueue.h"
#include "task.h"

taskPoolType taskPool = {0};
taskHandleType *currentTask;
taskHandleType *nextTask;

/**
 * @brief Function to execute when task returns
 *
 */
static void taskExitFunction()
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

    if (pTask->status == TASK_STATUS_BLOCKED)
    {
        /* Remove  task from the queue of blocked tasks*/
        taskQueueRemove(&taskPool.blockedQueue, pTask);
    }
    pTask->status = TASK_STATUS_READY;
    pTask->blockedReason = BLOCK_REASON_NONE;
    pTask->wakeupReason = wakeupReason;
    pTask->remainingSleepTicks = 0;

    /* Add task to queue of ready tasks*/
    taskQueueAdd(&taskPool.readyQueue, pTask);
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

    ENTER_CRITICAL_SECTION();

    pTask->remainingSleepTicks = ticks;
    pTask->status = TASK_STATUS_BLOCKED;
    pTask->blockedReason = blockedReason;
    pTask->wakeupReason = WAKEUP_REASON_NONE;

    // Add task to queue of blocked tasks. We dont need to sort tasks in blockedQueue
    taskQueueAddToFront(&taskPool.blockedQueue, pTask);

    EXIT_CRITICAL_SECTION();

    // Give CPU to other tasks
    taskYield();
}

/**
 * @brief Suspend task
 *
 * @param pTask Pointer to taskHandle struct.
 */
void taskSuspend(taskHandleType *pTask)
{
    assert(pTask != NULL);

    ENTER_CRITICAL_SECTION();

    /* If task status is ready, remove it from the readyQueue*/
    if (pTask->status == TASK_STATUS_READY)
    {
        taskQueueRemove(&taskPool.readyQueue, pTask);
    }
    /*If task status is blocked, remove it from the blockedQueue*/
    else if (pTask->status == TASK_STATUS_BLOCKED)
    {
        taskQueueRemove(&taskPool.blockedQueue, pTask);
    }

    pTask->remainingSleepTicks = 0;
    pTask->status = TASK_STATUS_SUSPENDED;
    pTask->blockedReason = BLOCK_REASON_NONE;
    pTask->wakeupReason = WAKEUP_REASON_NONE;

    EXIT_CRITICAL_SECTION();

    /*If self suspended, give CPU to other tasks*/
    if (pTask == taskPool.currentTask)
    {
        taskYield();
    }
}

/**
 * @brief Resume task from suspended state
 *
 * @param pTask Pointer to taskHandle struct
 * @return RET_SUCCESS if task resumed succesfully
 * @return RET_NOTSUSPENDED if task is not suspended
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
 * @brief Block task for specified number of OS Ticks
 *
 * @param sleepTicks
 * @retval RET_SUCCESS if task put to sleep successfullly
 * @retval RET_NOTACTIVE if task is not running
 */
static inline int taskSleep(uint32_t sleepTicks)
{
    taskHandleType *currentTask = taskPool.currentTask;
    if (currentTask->status == TASK_STATUS_RUNNING)
    {
        taskBlock(currentTask, SLEEP, sleepTicks);

        return RET_SUCCESS;
    }
    return RET_NOTACTIVE;
}

/**
 * @brief Block task for specified number of milliseconds
 *
 * @param sleepTimeMS
 * @retval RET_SUCCESS if task put to sleep successfullly
 * @retval RET_NOTACTIVE if task is not running
 */
int taskSleepMS(uint32_t sleepTimeMS)
{

    return taskSleep(MS_TO_OS_TICKS(sleepTimeMS));
}

/**
 * @brief Block task for specified number of microseconds
 *
 * @param sleepTimeUS
 * @retval RET_SUCCESS if task put to sleep successfullly
 * @retval RET_NOTACTIVE if task is not running
 */
int taskSleepUS(uint32_t sleepTimeUS)
{
    return taskSleep(US_TO_OS_TICKS(sleepTimeUS));
}

/**
 * @brief Initialize task's default stack contents and store pointer to taskHandle struct to pool of tasks ready to run. Calling this
 * function from main does not start execution of the task if Scheduler is not started.To start executeion of task, osStartScheduler must be
 * called from  main after calling taskStart. If this function is called from other running tasks, execution happens based on priority of the task.
 *
 * @param pTask Pointer to taskHandle struct
 */
void taskStart(taskHandleType *pTask)
{
    assert(pTask != NULL);
    /**********--Task's default stack values--****************************************
        ____ <-- stackBase
       |____|xPSR  --> stackPointer + 16
       |____|Return address(PC) <-Task Entry --> stackPointer + 15
       |____|LR --> stackPointer + 14
       |____|R12
       |____|R3
       |____|R2
       |____|R1
       |____|R0 <-Task params --> stackPointer + 9
       |____|EXC_RETURN --> stackPointer + 8

      <--Cortex-M3/M4/M7-->               <--Cortex-MO/M0+--->
       |____|R11                                 |____|R7
       |____|R10                                 |____|R6
       |____|R9                                  |____|R5
       |____|R8                                  |____|R4
       |____|R7                                  |____|R11
       |____|R6                                  |____|R10
       |____|R5                                  |____|R9
       |____|R4 <--stackPointer=(stackBase - 17) |____|R8 <--stackPointer=(stackBase - 17)
          |                                         |
          |                                         |
       |____|                                    |____|
       |____|                                    |____|
     <-32bits->                                 <-32bits->
    *************************************************************************************/

    *((uint32_t *)pTask->stackPointer + 8) = EXC_RETURN_THREAD_PSP;
    *((uint32_t *)pTask->stackPointer + 9) = (uint32_t)pTask->params;
    *((uint32_t *)pTask->stackPointer + 14) = (uint32_t)taskExitFunction;
    *((uint32_t *)pTask->stackPointer + 15) = (uint32_t)pTask->taskEntry;
    *((uint32_t *)pTask->stackPointer + 16) = 0x01000000; // Default xPSR register value

    /*Store pointer to the taskHandle struct to ready queue*/
    taskQueueAdd(&taskPool.readyQueue, pTask);
}
