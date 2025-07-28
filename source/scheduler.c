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
#include "sanoRTOS/log.h"

LOG_MODULE_DEFINE(scheduler);

TASK_DEFINE(idleTask0, 1024, idleTaskHandler0, NULL, TASK_LOWEST_PRIORITY, AFFINITY_CORE_0);

static atomic_t lock;

void idleTaskHandler0(void *params)
{
    (void)params;
    while (1)
    {
        // Enter sleep mode
        PORT_ENTER_SLEEP_MODE();
    }
}

static bool selectNextTask(void)
{
    taskQueueType *const pReadyQueue = getReadyQueue();

    if (!taskQueueEmpty(pReadyQueue))
    {
        taskHandleType *const pCurrentTask = taskGetCurrent();

        // If the current task is running, add it to the ready queue
        if (pCurrentTask->status == TASK_STATUS_RUNNING)
        {
            taskHandleType *pNextReadyTask = TASK_PEEK_FROM_READY_QUEUE(pReadyQueue);

            //If next ready task exists and has an equal or higher priority than current task,
            // add the current task to the ready queue.
            if (pNextReadyTask != NULL && pNextReadyTask->priority <= pCurrentTask->priority)
            {
                pCurrentTask->status = TASK_STATUS_READY;
                taskQueueAdd(pReadyQueue, pCurrentTask);
            }
            else
            {
                return false; // No need to switch to a lower priority task
            }
        }

        // Set the current task to the next ready task
        currentTask[PORT_CORE_ID()] = pCurrentTask;

#if CONFIG_CHECK_STACK_OVERFLOW
        // Check for stack overflow
        taskCheckStackOverflow();
#endif

        // Get the next task from the ready queue
        nextTask[PORT_CORE_ID()] = TASK_GET_FROM_READY_QUEUE(pReadyQueue);

        // Set the status of the next task to RUNNING
        nextTask[PORT_CORE_ID()]->status = TASK_STATUS_RUNNING;

        // Set the current task to the next task
        taskSetCurrent(nextTask[PORT_CORE_ID()]);

        return true;
    }

    return false;
}

static void checkTimeout()
{
    taskQueueType *pBlockedQueue = getBlockedQueue();

    /* Iterate over each task in the blocked queue */
    taskNodeType *currentTaskNode = pBlockedQueue->head;

    while (currentTaskNode != NULL)
    {
        /* Save next task node to avoid losing track of linked list after task node
         * is freed while setting corresponding task to READY */
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

void taskYield()
{
    bool contextSwitchRequired = false;

    bool irqState = spinLock(&lock);

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
    spinUnlock(&lock, irqState);
}

void tickHandler()
{

    bool irqState = spinLock(&lock);

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
    spinUnlock(&lock, irqState);
}

void schedulerStart()
{
    /*Start timerTask*/
    timerTaskStart();

    /* Start the idle task0*/
    taskStart(&idleTask0);

    portSchedulerStart();
}
