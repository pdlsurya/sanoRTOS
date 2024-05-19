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
#include "timer/timer.h"
#include "taskQueue/taskQueue.h"
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
    if (taskPool.readyQueue.head)
    {
        currentTask = taskPool.currentTask;

        if (currentTask->status == TASK_STATUS_RUNNING)
        {
            currentTask->status = TASK_STATUS_READY;
            taskQueueAdd(&taskPool.readyQueue, taskPool.currentTask);
        }

        // Get the next highest priority task
        nextTask = taskQueueGet(&taskPool.readyQueue);

        taskPool.currentTask = nextTask;

        nextTask->status = TASK_STATUS_RUNNING;

        // Perform context switch only if next task to execute is different than the current task
        if (currentTask != nextTask)
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

    taskNodeType *currentTaskNode = taskPool.blockedQueue.head;

    while (currentTaskNode != NULL)
    {
        if (currentTaskNode->pTask->remainingSleepTicks > 0)
        {
            currentTaskNode->pTask->remainingSleepTicks--;
            if (currentTaskNode->pTask->remainingSleepTicks == 0)
            {
                if (currentTaskNode->pTask->blockedReason == SLEEP)
                    taskSetReady(currentTaskNode->pTask, SLEEP_TIME_TIMEOUT);
                else
                    taskSetReady(currentTaskNode->pTask, WAIT_TIMEOUT);
            }
        }
        currentTaskNode = currentTaskNode->nextTaskNode;
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

    /* Assign lowest priority to PendSV*/
    NVIC_SetPriority(PendSV_IRQn, 0xff);

    /* Configure SysTick to generate interrupt every OS_INTERVAL_CPU_TICKS */
    SYSTICK_CONFIG();

    /*Get the highest priority ready task from ready Queue*/
    currentTask = taskPool.currentTask = taskQueueGet(&taskPool.readyQueue);

    /*Change status to RUNNING*/
    currentTask->status = TASK_STATUS_RUNNING;

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

    if (!taskQueueEmpty(&taskPool.blockedQueue))
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
