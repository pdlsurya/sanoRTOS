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

#include <stdlib.h>
#include "osConfig.h"
#include "task/task.h"
#include "timer/timer.h"
#include "taskQueue/taskQueue.h"
#include "scheduler.h"

#define IDLE_TASK_PRIORITY TASK_LOWEST_PRIORITY // Idle task has lowest possible priority[higher the value lower the priority]

TASK_DEFINE(idleTask, 192, idleTaskHandler, NULL, IDLE_TASK_PRIORITY);

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
 * @brief Select next highest priority ready task for execution and trigger PendSV to perform actual context switch.
 */
static void scheduleNextTask()
{
    if (!taskQueueEmpty(&taskPool.readyQueue))
    {

        if (taskPool.currentTask->status == TASK_STATUS_RUNNING)
        {
            /*Perform context switch only if next highest priority ready task has equal or higher priority[lower priority value]
            than the current running task*/

            taskHandleType *nextReadyTask = taskQueuePeek(&taskPool.readyQueue);

            if (nextReadyTask->priority <= taskPool.currentTask->priority)
            {
                /*Change current task's status to ready and add it to the readyQueue*/
                taskPool.currentTask->status = TASK_STATUS_READY;
                taskQueueAdd(&taskPool.readyQueue, taskPool.currentTask);
            }
            else
            {
                /*Current running task has higher priority than the next highest priority ready task;Hence, no need to perform context
                  switch. Return from here */
                return;
            }
        }

        currentTask = taskPool.currentTask;

        // Get the next highest priority  ready task
        nextTask = taskQueueGet(&taskPool.readyQueue);

        taskPool.currentTask = nextTask;

        nextTask->status = TASK_STATUS_RUNNING;

        /* Trigger PendSV to perform  context switch after all pending ISRs have been serviced: */
        triggerPendSV();
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
        /*Save next task node to avoid losing track of linked list after task node
         is freed while setting corresponding task to READY */
        taskNodeType *nextTaskNode = currentTaskNode->nextTaskNode;

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
        currentTaskNode = nextTaskNode;
    }
}

/**
 * @brief Function to voluntarily relinquish control of the CPU to allow other tasks to execute.
 */
void taskYield()
{
#if (TASK_RUN_PRIVILEGED)

    __disable_irq();

    scheduleNextTask();

    __enable_irq();

#else
    /*We need to be in privileged mode to trigger PendSV interrupt, We can use SVC call
     to switch to privileged mode and trigger PendSV interrupt from SVC handler.
     */
    SYSCALL(CONTEXT_SWITCH);
#endif
}

/**
 * @brief Function to start the RTOS task scheduler.
 */
void schedulerStart()
{
    /*Start timerTask*/
    timerTaskStart();

    /* Start the idle task*/
    taskStart(&idleTask);

    /* Assign lowest priority to PendSV*/
    NVIC_SetPriority(PendSV_IRQn, 0xff);

    /* Assign lowest priority to SysTick*/
    NVIC_SetPriority(SysTick_IRQn, SYSTICK_PRIORITY);

    /* Configure SysTick to generate interrupt every OS_INTERVAL_CPU_TICKS */
    SYSTICK_CONFIG();

    /*Get the highest priority ready task from ready Queue*/
    currentTask = taskPool.currentTask = taskQueueGet(&taskPool.readyQueue);

    /*Change status to RUNNING*/
    currentTask->status = TASK_STATUS_RUNNING;

    /* Set PSP to the top of task's stack */
    __set_PSP(currentTask->stackPointer);

    /* Switch to Unprivileged or Privileged  Mode depending on OS_RUN_PRIV flag with PSP as the stack pointer */
    __set_CONTROL(TASK_RUN_PRIVILEGED ? 0x02 : 0x03);

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

    /*Check for timer timeout*/
    processTimers();

    /*Check for wait timeout of blocked tasks*/
    if (!taskQueueEmpty(&taskPool.blockedQueue))
    {
        checkTimeout();
    }

    /*Perform context switch if required*/
    scheduleNextTask();

    __enable_irq();
}

void SVC_Handler()
{

    /*Get the stack pointer of the task which triggered the SVC interrupt*/
    uint32_t *stackPointer = (uint32_t *)__get_PSP();

    /*Get PC from exception stack frame which hold address of instruction after the SVC instruction
        ____ <-- stackBase
       |____|xPSR stackPointer[7]
       |____|PC stackPointer[6]
       |____|LR stackPointer[5]
       |____|R12 stackPointer[4]
       |____|R3 stackPointer[3]
       |____|R2 stackPointer[2]
       |____|R1 stackPointer[1]
       |____|R0 stackPointer[0]*/

    uint32_t PC = stackPointer[6];

    /*Extract the SVC number from the SVC instruction, which resides just before the address in the PC.
     The SVC instruction is a 16-bit instruction, whose LSB byte is the SVC number.
     Hence, the SVC number is extracted by accessing the value at the memory location which is two bytes before the address in the PC.*/
    uint8_t svcNumber = *((uint8_t *)PC - 2);

    switch (svcNumber)
    {
    case DISABLE_INTERRUPTS:
        /*Disable all the interrupts whose priority value is 1 or higher*/
        __set_BASEPRI(1);
        break;
    case ENABLE_INTERUPPTS:
        /*Enable all the interrupts*/
        __set_BASEPRI(0);
        break;
    case CONTEXT_SWITCH:
        scheduleNextTask();
        break;
    default:
        break;
    }
}
