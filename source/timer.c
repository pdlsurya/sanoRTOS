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

#include <stdbool.h>
#include <stdint.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/scheduler.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/timer.h"
#include "sanoRTOS/mem.h"

#define TIMER_TASK_PRIORITY TASK_HIGHEST_PRIORITY // timer task has the highest possible priority [lower the value, higher the priority]

static timerListType timerList = {0}; // List of running timers

static timeoutHandlerQueueType timeoutHandlerQueue = {0}; // Queue of timeout handlers to be executed

/*Define timer task with highest possible priority*/
TASK_DEFINE(timerTask, 4096, timerTaskFunction, NULL, TIMER_TASK_PRIORITY, AFFINITY_CORE_0);

static int timeoutHandlerQueuePush(timeoutHandlerQueueType *pTimeoutHandlerQueue, timeoutHandlerType timeoutHandler)
{
    timeoutHandlerNodeType *newNode = (timeoutHandlerNodeType *)memAlloc(sizeof(timeoutHandlerNodeType));
    if (newNode == NULL)
    {
        return RET_NOMEM;
    }

    newNode->timeoutHandler = timeoutHandler;
    newNode->nextNode = NULL;

    if (pTimeoutHandlerQueue->head == NULL)
    {
        pTimeoutHandlerQueue->head = newNode;
        pTimeoutHandlerQueue->tail = pTimeoutHandlerQueue->head;
    }
    else
    {
        pTimeoutHandlerQueue->tail->nextNode = newNode;
        pTimeoutHandlerQueue->tail = newNode;
    }

    return RET_SUCCESS;
}

static timeoutHandlerType timeoutHandlerQueuePop(timeoutHandlerQueueType *pTimeoutHandlerQueue)
{

    timeoutHandlerNodeType *temp = pTimeoutHandlerQueue->head->nextNode;

    timeoutHandlerType timeoutHandler = pTimeoutHandlerQueue->head->timeoutHandler;

    memFree(pTimeoutHandlerQueue->head);

    pTimeoutHandlerQueue->head = temp;

    return timeoutHandler;
}

static void timerListNodeAdd(timerListType *pTimerList, timerNodeType *pTimerNode)
{

    pTimerNode->nextNode = pTimerList->head;

    pTimerList->head = pTimerNode;
}

static inline void timerListDeleteFirstNode(timerListType *pTimerList)
{
    timerNodeType *tempNode = pTimerList->head->nextNode;

    pTimerList->head->nextNode = NULL;

    pTimerList->head = tempNode;
}

static int timerListNodeDelete(timerListType *pTimerList, timerNodeType *pTimerNode)
{

    if (pTimerList->head != NULL) // Check if timerList is empty
    {
        timerNodeType *currentNode = pTimerList->head;

        /* If the timer correponds to the head node in the list, remove the timer node and reassign head node.*/
        if (pTimerNode == pTimerList->head)
        {
            timerListDeleteFirstNode(pTimerList);
        }

        else
        {
            while (currentNode->nextNode != pTimerNode)
            {
                currentNode = currentNode->nextNode;
            }

            currentNode->nextNode = pTimerNode->nextNode;
            pTimerNode->nextNode = NULL;
        }
        return RET_SUCCESS;
    }
    return RET_EMPTY;
}

int timerStart(timerNodeType *pTimerNode, uint32_t intervalTicks)
{
    if (pTimerNode == NULL)
    {
        return RET_INVAL;
    }

    /* check if the timer is already in running state. If so, abort re-starting the timer.*/
    if (pTimerNode->isRunning)
    {
        return RET_ALREADYACTIVE;
    }

    /* Set isRunning flag for the started timer pTimerNode.*/
    pTimerNode->isRunning = true;

    pTimerNode->ticksToExpire = pTimerNode->intervalTicks = intervalTicks;

    /* Add the timer in the queue of running timers*/
    timerListNodeAdd(&timerList, pTimerNode);

    return RET_SUCCESS;
}

int timerStop(timerNodeType *pTimerNode)
{
    if (pTimerNode == NULL)
    {
        return RET_INVAL;
    }

    if (pTimerNode->isRunning)
    {
        pTimerNode->isRunning = false;
        return timerListNodeDelete(&timerList, pTimerNode);
    }
    return RET_NOTACTIVE;
}

void processTimers()
{
    if (timerList.head != NULL) // Check if timer list is empty
    {
        timerNodeType *currentNode = timerList.head;
        while (currentNode != NULL)
        {
            /*Save next node to avoid losing track of linked list after a time node is deleted while stopping the timer*/
            timerNodeType *nextNode = currentNode->nextNode;

            /*Decrement ticks to expire*/
            if (currentNode->ticksToExpire > 0)
            {
                currentNode->ticksToExpire--;

                /* Check if timer has expired.If set, call the timeoutHandler() and update the ticksToExpire for next event.*/
                if (currentNode->ticksToExpire == 0)
                {

                    /*Add timeout handler to the timeoutHandlerQueue*/
                    if (timeoutHandlerQueuePush(&timeoutHandlerQueue, currentNode->timeoutHandler) == RET_SUCCESS)
                    {
                        /* Check if timer task is BLOCKED. If so, change status to ready to allow execution.*/
                        if (timerTask.status == TASK_STATUS_BLOCKED)
                        {
                            (void)taskSetReady(&timerTask, TIMER_TIMEOUT);
                        }

                        currentNode->ticksToExpire = currentNode->intervalTicks;
                    }

                    /* Check if the timer mode is SINGLE_SHOT. If true, stop the correponding timer*/
                    if (currentNode->mode == TIMER_MODE_SINGLE_SHOT)
                    {
                        timerStop(currentNode);
                    }
                }
            }
            currentNode = nextNode;
        }
    }
}

int timerTaskStart()
{
    return taskStart(&timerTask);
}

void timerTaskFunction(void *args)
{
    (void)args;

    while (1)
    {
        if (timeoutHandlerQueue.head != NULL) // Check for non-empty condition
        {
            timeoutHandlerType timeoutHandler = timeoutHandlerQueuePop(&timeoutHandlerQueue);
            timeoutHandler();
        }
        else
        {
            /* Block timer task and give cpu to other tasks while waiting for timeout*/
            (void)taskBlock(&timerTask, WAIT_FOR_TIMER_TIMEOUT, 0);
        }
    }
}
