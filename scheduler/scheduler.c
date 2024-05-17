/**
 * @file scheduler.c
 * @author Surya Poudel
 * @brief
 * @version 0.1
 * @date 2024-04-09
 *
 * @copyright Copyright (c) 2024
 *
 */
#include <stdlib.h>
#include "osConfig.h"
#include "task/task.h"
#include "queue/queue.h"
#include "mutex/mutex.h"
#include "timer/timer.h"
#include "utils/utils.h"
#include "scheduler.h"

#define IDLE_TASK_PRIORITY TASK_LOWEST_PRIORITY // Idle task has lowest possible priority[higher the value lower the priority]

TASK_DEFINE(idleTask, 48, idleTaskHandler, NULL, IDLE_TASK_PRIORITY);

void idleTaskHandler(void *params)
{
    (void)params;
    while (1)
        ;
}
/**
 * @brief Trigger PendSV interrupt
 *
 */
static inline void triggerPendSV()
{
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

/**
 * @brief Select next ready task for execution and trigger PendSV to perform actual context switch.
 */
static void scheduleNextTask()
{
    if (taskPool.readyTasksCount > 0)
    {
        currentTask = taskPool.tasks[taskPool.currentTaskId];

        if (currentTask->status == TASK_STATUS_RUNNING)
            currentTask->status = TASK_STATUS_READY;

        memset(taskPool.readyTasks, 0, sizeof(taskPool.readyTasks));

        uint8_t index = 0;

        for (int taskId = 0; taskId < (taskPool.readyTasksCount + taskPool.blockedTasksCount + taskPool.suspendedTasksCount); taskId++)
        {
            if (taskPool.tasks[taskId]->status == TASK_STATUS_READY)
            {
                taskPool.readyTasks[index++] = taskPool.tasks[taskId];
            }
        }

        // Sort list of READY tasks in ascending order of their priority
        qsort(taskPool.readyTasks, taskPool.readyTasksCount, sizeof(taskHandleType *), sortCompareFunction);

        // Get the first highest priority task from the list of READY tasks
        taskHandleType *temp = taskPool.readyTasks[0];

        /*If current task's priority is not equal to the priority of first task in the READY tasks list,
         *next task will be the highest priority task[i.e. first task in the READY tasks list]
         */
        if (currentTask->priority != temp->priority)
        {
            taskPool.currentTaskId = temp->taskId;
        }
        /*
         *If current task is the highest priority task, we need to check if there are any other tasks in the READY tasks list with
         *the same priority as the current highest priority task. If so, Round-robin scheduling is performed to provide equal chance to
         *all tasks with same priority.
         */
        else
        {
            // Get the index of current task in the READY tasks list.
            int currentTaskIndex = getTaskIndex(taskPool.readyTasks, taskPool.readyTasksCount, currentTask);

            int nextTaskIndex = (currentTaskIndex + 1) % taskPool.readyTasksCount;

            // Round-robin scheduling
            if (taskPool.readyTasks[nextTaskIndex]->priority == temp->priority)
            {
                taskPool.currentTaskId = taskPool.readyTasks[nextTaskIndex]->taskId;
            }
            else
                taskPool.currentTaskId = temp->taskId;
        }

        // Select next task to execute and change its status to RUNNING.
        nextTask = taskPool.tasks[taskPool.currentTaskId];
        nextTask->status = TASK_STATUS_RUNNING;

        // Perform context switch only if next task to execute is different than the current task
        if (nextTask->taskId != currentTask->taskId)
        {
            /* Trigger PendSV to perform  context switch after all pending ISRs have been serviced: */
            triggerPendSV();
        }
    }
}

/**
 * @brief Check for timeout of blocked tasks and change  status to READY
 * with corresponding timeout reason.
 */
static void checkTimeout()
{
    for (int taskId = 0; taskId < (taskPool.readyTasksCount + taskPool.suspendedTasksCount + taskPool.blockedTasksCount); taskId++)
    {
        if (taskPool.tasks[taskId]->status == TASK_STATUS_BLOCKED)
        {
            if (taskPool.tasks[taskId]->remainingSleepTicks > 0)
            {
                taskPool.tasks[taskId]->remainingSleepTicks--;
                if (taskPool.tasks[taskId]->remainingSleepTicks == 0)
                {
                    if (taskPool.tasks[taskId]->blockedReason == SLEEP)
                        taskSetReady(taskPool.tasks[taskId], SLEEP_TIME_TIMEOUT);
                    else
                        taskSetReady(taskPool.tasks[taskId], WAIT_TIMEOUT);
                }
            }
        }
    }
}

/**
 * @brief Function to voluntarily relinquish control of the CPU to allow other tasks to execute.
 */
void taskYield()
{
    if (TASKS_RUN_PRIV)
    {
        __disable_irq();

        scheduleNextTask();

        __enable_irq();
    }

    /*We need to be in privileged mode to trigger PendSV interrupt, We can use SVC call
     to switch to privileged mode and trigger PendSV interrupt from SVC handler.
     */
    else
        __asm volatile("svc #0xff");
}

/**
 * @brief Function to start the RTOS task scheduler.
 */
void osStartScheduler()
{
    /*Start timerTask*/
    timerTaskStart();

    /* Start the idle task*/
    taskStart(&idleTask);

    /* Last task inserted into the task pool before starting the scheduler is idle task,
     hence, get id of the idle task and prepare to run at first.
     */
    taskPool.currentTaskId = taskPool.taskInsertIndex - 1;
    currentTask = taskPool.tasks[taskPool.currentTaskId];

    /* Since idleTask is the first task to run, we do not need to store default stack values,
     *hence advance stack pointer by 64 bytes  */
    currentTask->stackPointer = currentTask->stackPointer + 17 * 4;

    /* Assign lowest priority to PendSV*/
    NVIC_SetPriority(PendSV_IRQn, 0xff);

    /* Configure SysTick to generate interrupt every OS_INTERVAL_CPU_TICKS */
    SYSTICK_CONFIG();

    /* Set PSP to the top of task's stack */
    __set_PSP(currentTask->stackPointer);

    /* Switch to Unprivileged or Privileged  Mode depending on OS_RUN_PRIV flag with PSP as the stack pointer */
    __set_CONTROL(TASKS_RUN_PRIV ? 0x02 : 0x03);

    /* Execute ISB after changing CONTORL register */
    __ISB();

    currentTask->taskEntry(currentTask->params);
}

/**
 * @brief SysTick Timer interrupt handler. It selects next task to run and
 * triggers PendSV to perform actual context switch.
 */
void SYSTICK_HANDLER()
{
    __disable_irq();

    processTimers();

    if (taskPool.blockedTasksCount > 0)
        checkTimeout();

    scheduleNextTask();

    __enable_irq();
}

void SVC_Handler()
{
    /*Now we are in privileged mode. We can select next task and trigger
    PendSV interrupt to perform actual context switch.
    */
    __disable_irq();

    scheduleNextTask();

    __enable_irq();
}
