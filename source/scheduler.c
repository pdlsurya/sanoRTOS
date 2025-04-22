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
#include "sanoRTOS/scheduler.h"
#include "sanoRTOS/config.h"
#include "sanoRTOS/spinLock.h"
#include "sanoRTOS/port.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/timer.h"
#include "sanoRTOS/taskQueue.h"

TASK_DEFINE(idleTask0, 1024, idleTaskHandler0, NULL, TASK_LOWEST_PRIORITY, AFFINITY_CORE_0);

static atomic_t lock;

void idleTaskHandler0(void *params)
{
    (void)params;
    while (1)
    {
        PORT_ENTER_SLEEP_MODE();
    }
}

/**
 * @brief Select next highest priority ready task for execution
 *
 * @retval TRUE if context switch  required
 * @retval FALSE  if context switch not required
 */
static bool selectNextTask()
{
    taskQueueType *pReadyQueue = getReadyQueue();

    if (!taskQueueEmpty(pReadyQueue))
    {
        // Current Running task
        taskHandleType *pTask = taskGetCurrent();

        if (pTask->status == TASK_STATUS_RUNNING)
        {
            /*Perform context switch only if next highest priority ready task has equal or higher priority[lower priority value]
            than the current running task*/

            taskHandleType *nextReadyTask = taskQueuePeek(pReadyQueue);

            if (nextReadyTask->priority <= pTask->priority)
            {
                /*Change current task's status to ready and add it to the readyQueue*/
                pTask->status = TASK_STATUS_READY;
                taskQueueAdd(pReadyQueue, pTask);
            }
            else
            {
                /*Current running task has higher priority than the next highest priority ready task;Hence, no need to perform context
                  switch.*/
                return false;
            }
        }

        currentTask[PORT_CORE_ID()] = pTask;

        taskCheckStackOverflow();

        // Get the next highest priority  ready task
        nextTask[PORT_CORE_ID()] = taskQueueGet(pReadyQueue);

        nextTask[PORT_CORE_ID()]->status = TASK_STATUS_RUNNING;

        taskSetCurrent(nextTask[PORT_CORE_ID()]);

        // Context switch required
        return true;
    }

    // No tasks in ready state, Context swtich not required
    return false;
}

/**
 * @brief Check for timeout of blocked tasks and change  status to READY
 * with corresponding timeout reason.
 */
static void checkTimeout()
{
    taskQueueType *pBlockedQueue = getBlockedQueue();

    taskNodeType *currentTaskNode = pBlockedQueue->head;

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
                {
                    taskSetReady(currentTaskNode->pTask, SLEEP_TIME_TIMEOUT);
                }
                else
                {
                    taskSetReady(currentTaskNode->pTask, WAIT_TIMEOUT);
                }
            }
        }
        currentTaskNode = nextTaskNode;
    }
}

/**
 * @brief Voluntarily relinquish control of the CPU to allow other tasks to execute.
 */
void taskYield()
{
    bool contextSwitchRequired = false;

    bool irqFlag = spinLock(&lock);

    contextSwitchRequired = selectNextTask();

    if (contextSwitchRequired)
    {

#if (CONFIG_TASK_USER_MODE)
        /*We need to be in privileged mode to perform context switch.*/
        PORT_SYSCALL(SWITCH_CONTEXT);

#else
        /*Trigger platform specific context switch mechanism*/
        PORT_TRIGGER_CONTEXT_SWITCH();

#endif
    }
    spinUnlock(&lock, irqFlag);
}

/**
 * @brief RTOS Tick interrupt handler. It selects next task to run and
 * triggers platform specific context switch mechanism.
 */
void tickHandler()
{

    bool irqFlag = spinLock(&lock);

    bool contextSwitchRequired = false;

    /*Handle timers and task timeouts on Core 0 only*/
    if (PORT_CORE_ID() == 0)
    {
        /*Check for timer timeout*/
        processTimers();

        taskQueueType *pBlockedQueue = getBlockedQueue();

        /*Check for wait timeout of blocked tasks*/
        if (!taskQueueEmpty(pBlockedQueue))
        {
            checkTimeout();
        }
    }

    /*Perform context switch if required*/
    contextSwitchRequired = selectNextTask();

    if (contextSwitchRequired)
    {
        /*Trigger platform specific context switch mechanism*/
        PORT_TRIGGER_CONTEXT_SWITCH();
    }
    spinUnlock(&lock, irqFlag);
}

/**
 * @brief Function to start the RTOS task scheduler.
 */
void schedulerStart()
{
    /*Start timerTask*/
    timerTaskStart();

    /* Start the idle task0*/
    taskStart(&idleTask0);

    portSchedulerStart();
}
