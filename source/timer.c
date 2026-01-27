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
#include "sanoRTOS/memSlab.h"
#include "sanoRTOS/spinLock.h"

#define TIMER_TASK_PRIORITY TASK_HIGHEST_PRIORITY // timer task has the highest possible priority [lower the value, higher the priority]

static timerListType timerList = {0}; // List of running timers

static timeoutHandlerQueueType timeoutHandlerQueue = {0}; // Queue of timeout handlers to be executed

static atomic_t lock; // Protects timer list and timeout handler queue

MEM_SLAB_DEFINE(timeoutHandlerNodeSlab, sizeof(timeoutHandlerNodeType), CONFIG_TIMER_TIMEOUT_NODE_SLAB_BLOCKS);

/*Define timer task with highest possible priority*/
TASK_DEFINE(timerTask, 4096, timerTaskFunction, NULL, TIMER_TASK_PRIORITY, AFFINITY_CORE_0);

static int timeoutHandlerQueuePush(timeoutHandlerQueueType *pTimeoutHandlerQueue,
                                   timeoutHandlerType timeoutHandler, void *pArg)
{
    timeoutHandlerNodeType *newNode = NULL;
    if (memSlabAlloc(&timeoutHandlerNodeSlab, (void **)&newNode, TASK_NO_WAIT) != RET_SUCCESS)
    {
        return RET_NOMEM;
    }

    newNode->timeoutHandler = timeoutHandler;
    newNode->pArg = pArg;
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

static timeoutHandlerType timeoutHandlerQueuePop(timeoutHandlerQueueType *pTimeoutHandlerQueue, void **ppArg)
{
    if ((pTimeoutHandlerQueue == NULL) || (pTimeoutHandlerQueue->head == NULL))
    {
        if (ppArg != NULL)
        {
            *ppArg = NULL;
        }
        return NULL;
    }

    timeoutHandlerNodeType *temp = pTimeoutHandlerQueue->head->nextNode;

    timeoutHandlerNodeType *pNode = pTimeoutHandlerQueue->head;
    timeoutHandlerType timeoutHandler = pNode->timeoutHandler;
    if (ppArg != NULL)
    {
        *ppArg = pNode->pArg;
    }

    pTimeoutHandlerQueue->head = temp;
    if (pTimeoutHandlerQueue->head == NULL)
    {
        pTimeoutHandlerQueue->tail = NULL;
    }

    (void)memSlabFree(&timeoutHandlerNodeSlab, pNode);

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

static int timerStopLocked(timerNodeType *pTimerNode)
{
    if (pTimerNode->isRunning)
    {
        pTimerNode->isRunning = false;
        return timerListNodeDelete(&timerList, pTimerNode);
    }

    return RET_NOTACTIVE;
}

int timerStart(timerNodeType *pTimerNode, uint32_t intervalTicks)
{
    if (pTimerNode == NULL)
    {
        return RET_INVAL;
    }

    int retCode = RET_SUCCESS;
    bool irqState = spinLock(&lock);

    /* check if the timer is already in running state. If so, abort re-starting the timer.*/
    if (pTimerNode->isRunning)
    {
        retCode = RET_ALREADYACTIVE;
    }
    else
    {
        /* Set isRunning flag for the started timer pTimerNode.*/
        pTimerNode->isRunning = true;

        pTimerNode->ticksToExpire = pTimerNode->intervalTicks = intervalTicks;

        /* Add the timer in the queue of running timers*/
        timerListNodeAdd(&timerList, pTimerNode);
    }

    spinUnlock(&lock, irqState);

    return retCode;
}

int timerStop(timerNodeType *pTimerNode)
{
    if (pTimerNode == NULL)
    {
        return RET_INVAL;
    }

    bool irqState = spinLock(&lock);
    int retCode = timerStopLocked(pTimerNode);
    spinUnlock(&lock, irqState);

    return retCode;
}

void processTimers()
{
    bool irqState = spinLock(&lock);

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
                    if (timeoutHandlerQueuePush(&timeoutHandlerQueue, currentNode->timeoutHandler,
                                                currentNode->pArg) == RET_SUCCESS)
                    {
                        /* Notify timer task that timeout work is available. */
                        (void)taskNotify(&timerTask, 0U, TASK_NOTIFY_INCREMENT);

                        currentNode->ticksToExpire = currentNode->intervalTicks;
                    }

                    /* Check if the timer mode is SINGLE_SHOT. If true, stop the correponding timer*/
                    if (currentNode->mode == TIMER_MODE_SINGLE_SHOT)
                    {
                        (void)timerStopLocked(currentNode);
                    }
                }
            }
            currentNode = nextNode;
        }
    }

    spinUnlock(&lock, irqState);
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
        bool irqState = spinLock(&lock);
        void *pArg = NULL;
        timeoutHandlerType timeoutHandler = timeoutHandlerQueuePop(&timeoutHandlerQueue, &pArg);
        spinUnlock(&lock, irqState);

        if (timeoutHandler != NULL)
        {
            timeoutHandler(pArg);
        }
        else
        {
            /* Block until at least one timeout handler is queued. */
            (void)taskNotifyTake(true, NULL, TASK_MAX_WAIT);
        }
    }
}
