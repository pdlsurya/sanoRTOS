/**
 * @file task.c
 * @author Surya Poudel
 * @brief
 * @version 0.1
 * @date 2024-04-09
 *
 * @copyright Copyright (c) 2024
 *
 */

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
 * @brief Change a task's status to ready
 * @param pTask Pointer to the taskHandle struct.
 */
void taskSetReady(taskHandleType *pTask, wakeupReasonType wakeupReason)
{
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

    pTask->remainingSleepTicks = ticks;
    pTask->status = TASK_STATUS_BLOCKED;
    pTask->blockedReason = blockedReason;
    pTask->wakeupReason = WAKEUP_REASON_NONE;

    // Add task to queue of blocked tasks. We dont need to sort tasks in blockedQueue
    taskQueueAddToFront(&taskPool.blockedQueue, pTask);

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
    // If task is in ready queue, remove it from the queue
    if (pTask->status == TASK_STATUS_READY)
        taskQueueRemove(&taskPool.readyQueue, pTask);

    pTask->remainingSleepTicks = 0;
    pTask->status = TASK_STATUS_SUSPENDED;
    pTask->blockedReason = BLOCK_REASON_NONE;
    pTask->wakeupReason = WAKEUP_REASON_NONE;

    if (pTask == taskPool.currentTask)
        taskYield();
}

/**
 * @brief Resume task from suspended state
 *
 * @param pTask Pointer to taskHandle struct
 * @return true if task resumed succesfully
 * @return false if task resume failed
 */
bool taskResume(taskHandleType *pTask)
{
    if (pTask->status == TASK_STATUS_SUSPENDED)
    {
        taskSetReady(pTask, RESUME);
        return true;
    }
    return false;
}

/**
 * @brief Block task for specified number of OS Ticks
 *
 * @param sleepTicks
 */
static inline void taskSleep(uint32_t sleepTicks)
{
    taskHandleType *currentTask = taskPool.currentTask;
    if (currentTask->status == TASK_STATUS_RUNNING)
    {
        taskBlock(currentTask, SLEEP, sleepTicks);
    }
}

/**
 * @brief Block task for specified number of milliseconds
 *
 * @param sleepTimeMS
 */
void taskSleepMS(uint32_t sleepTimeMS)
{

    taskSleep(MS_TO_OS_TICKS(sleepTimeMS));
}

/**
 * @brief Block task for specified number of microseconds
 *
 * @param sleepTimeUS
 */
void taskSleepUS(uint32_t sleepTimeUS)
{
    taskSleep(US_TO_OS_TICKS(sleepTimeUS));
}
/**
 * @brief Initialize task's default stack contents and store pointer to taskHandle struct to pool of tasks ready to run. Calling this
 * function from main does not start execution of the task if Scheduler is not started.To start executeion of task, osStartScheduler must be
 * called from  main after calling taskStart. If this function is called from other running tasks, execution happens based on priority of the task.
 *
 * @param pTaskHandle Pointer to taskHandle struct
 * @return true if task started successfully
 * @return false  if task start failed
 */
bool taskStart(taskHandleType *pTaskHandle)
{

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
   |____|R11
   |____|R10
   |____|R9
   |____|R8
   |____|R7
   |____|R6
   |____|R5
   |____|R4 <--stackPointer=(stackBase - 17)
      |
      |
   |____|
   |____|
 <-32bits->
   *************************************************************************************/

    *((uint32_t *)pTaskHandle->stackPointer + 8) = EXC_RETURN_THREAD_PSP;
    *((uint32_t *)pTaskHandle->stackPointer + 9) = (uint32_t)pTaskHandle->params;
    *((uint32_t *)pTaskHandle->stackPointer + 14) = (uint32_t)taskExitFunction;
    *((uint32_t *)pTaskHandle->stackPointer + 15) = (uint32_t)pTaskHandle->taskEntry;
    *((uint32_t *)pTaskHandle->stackPointer + 16) = 0x01000000; // Default xPSR register value

    /*Store pointer to the taskHandle struct to ready queue*/
    taskQueueAdd(&taskPool.readyQueue, pTaskHandle);

    return true;
}
