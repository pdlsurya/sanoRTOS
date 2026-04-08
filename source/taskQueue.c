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

#include <stdint.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/taskQueue.h"

/* Selects which intrusive queue link inside a task a helper should operate on. */
typedef taskQueueLinkType *(*taskQueueLinkAccessorType)(taskHandleType *pTask);

static inline taskQueueLinkType *taskStateQueueLink(taskHandleType *pTask)
{
    return (pTask == NULL) ? NULL : &pTask->stateQueueLink;
}

static inline taskQueueLinkType *taskWaitQueueLink(taskHandleType *pTask)
{
    return (pTask == NULL) ? NULL : &pTask->waitQueueLink;
}

static inline taskQueueLinkType *taskTimeoutQueueLink(taskHandleType *pTask)
{
    return (pTask == NULL) ? NULL : &pTask->timeoutQueueLink;
}

static inline bool taskDeadlineBefore(uint32_t lhs, uint32_t rhs)
{
    return ((int32_t)(lhs - rhs) < 0);
}

static int taskQueueInsertBetween(taskQueueType *pQueue,
                                  taskHandleType *pPrevTask,
                                  taskHandleType *pNextTask,
                                  taskHandleType *pTask,
                                  taskQueueLinkAccessorType linkOf)
{
    taskQueueLinkType *pLink = NULL;

    if ((pQueue == NULL) || (pTask == NULL) || (linkOf == NULL))
    {
        return RET_INVAL;
    }

    pLink = linkOf(pTask);
    if ((pLink == NULL) || (pLink->pOwnerQueue != NULL))
    {
        return RET_INVAL;
    }

    pLink->pPrevTask = pPrevTask;
    pLink->pNextTask = pNextTask;
    pLink->pOwnerQueue = pQueue;

    if (pPrevTask != NULL)
    {
        linkOf(pPrevTask)->pNextTask = pTask;
    }
    else
    {
        pQueue->head = pTask;
    }

    if (pNextTask != NULL)
    {
        linkOf(pNextTask)->pPrevTask = pTask;
    }

    return RET_SUCCESS;
}

static int taskQueueRemove(taskQueueType *pQueue,
                           taskHandleType *pTask,
                           taskQueueLinkAccessorType linkOf)
{
    taskQueueLinkType *pLink = NULL;

    if ((pQueue == NULL) || (pTask == NULL) || (linkOf == NULL))
    {
        return RET_INVAL;
    }

    pLink = linkOf(pTask);
    if ((pLink == NULL) || (pLink->pOwnerQueue != pQueue))
    {
        return RET_NOTASK;
    }

    if (pLink->pPrevTask != NULL)
    {
        linkOf(pLink->pPrevTask)->pNextTask = pLink->pNextTask;
    }
    else
    {
        pQueue->head = pLink->pNextTask;
    }

    if (pLink->pNextTask != NULL)
    {
        linkOf(pLink->pNextTask)->pPrevTask = pLink->pPrevTask;
    }

    pLink->pPrevTask = NULL;
    pLink->pNextTask = NULL;
    pLink->pOwnerQueue = NULL;

    return RET_SUCCESS;
}

static int taskQueueRemoveOwned(taskHandleType *pTask, taskQueueLinkAccessorType linkOf)
{
    taskQueueLinkType *pLink = NULL;

    if ((pTask == NULL) || (linkOf == NULL))
    {
        return RET_INVAL;
    }

    pLink = linkOf(pTask);
    if ((pLink == NULL) || (pLink->pOwnerQueue == NULL))
    {
        return RET_NOTASK;
    }

    return taskQueueRemove(pLink->pOwnerQueue, pTask, linkOf);
}

static taskHandleType *taskQueueNextTask(taskQueueType *pQueue,
                                         taskHandleType *pTask,
                                         taskQueueLinkAccessorType linkOf)
{
    taskQueueLinkType *pLink = NULL;

    if ((pQueue == NULL) || (pTask == NULL) || (linkOf == NULL))
    {
        return NULL;
    }

    pLink = linkOf(pTask);
    if ((pLink == NULL) || (pLink->pOwnerQueue != pQueue))
    {
        return NULL;
    }

    return pLink->pNextTask;
}

static int taskQueueAddPrioritySorted(taskQueueType *pQueue,
                                      taskHandleType *pTask,
                                      taskQueueLinkAccessorType linkOf)
{
    taskHandleType *currentTask = NULL;
    taskHandleType *nextTask = NULL;

    if ((pQueue == NULL) || (pTask == NULL) || (linkOf == NULL))
    {
        return RET_INVAL;
    }

    if (taskQueueEmpty(pQueue))
    {
        return taskQueueInsertBetween(pQueue, NULL, NULL, pTask, linkOf);
    }

    if (pQueue->head->priority > pTask->priority)
    {
        return taskQueueInsertBetween(pQueue, NULL, pQueue->head, pTask, linkOf);
    }

    currentTask = pQueue->head;
    nextTask = taskQueueNextTask(pQueue, currentTask, linkOf);

    while ((nextTask != NULL) && (nextTask->priority <= pTask->priority))
    {
        currentTask = nextTask;
        nextTask = taskQueueNextTask(pQueue, currentTask, linkOf);
    }

    return taskQueueInsertBetween(pQueue, currentTask, nextTask, pTask, linkOf);
}

static int taskQueueAddDeadlineSorted(taskQueueType *pQueue,
                                      taskHandleType *pTask,
                                      taskQueueLinkAccessorType linkOf)
{
    taskHandleType *currentTask = NULL;
    taskHandleType *nextTask = NULL;

    if ((pQueue == NULL) || (pTask == NULL) || (linkOf == NULL))
    {
        return RET_INVAL;
    }

    if (taskQueueEmpty(pQueue))
    {
        return taskQueueInsertBetween(pQueue, NULL, NULL, pTask, linkOf);
    }

    if (taskDeadlineBefore(pTask->deadlineTick, pQueue->head->deadlineTick))
    {
        return taskQueueInsertBetween(pQueue, NULL, pQueue->head, pTask, linkOf);
    }

    currentTask = pQueue->head;
    nextTask = taskQueueNextTask(pQueue, currentTask, linkOf);

    while ((nextTask != NULL) && !taskDeadlineBefore(pTask->deadlineTick, nextTask->deadlineTick))
    {
        currentTask = nextTask;
        nextTask = taskQueueNextTask(pQueue, currentTask, linkOf);
    }

    return taskQueueInsertBetween(pQueue, currentTask, nextTask, pTask, linkOf);
}

static taskHandleType *taskQueuePop(taskQueueType *pQueue,
                                    bool affinityCheck,
                                    taskQueueLinkAccessorType linkOf)
{
    taskHandleType *currentTask = NULL;

    if ((pQueue == NULL) || (linkOf == NULL) || taskQueueEmpty(pQueue))
    {
        return NULL;
    }

    if (!affinityCheck)
    {
        taskHandleType *pTask = pQueue->head;
        return (taskQueueRemove(pQueue, pTask, linkOf) == RET_SUCCESS) ? pTask : NULL;
    }

    currentTask = pQueue->head;
    while (currentTask != NULL)
    {
        taskHandleType *nextTask = taskQueueNextTask(pQueue, currentTask, linkOf);

        if ((currentTask->coreAffinity == PORT_CORE_ID()) ||
            (currentTask->coreAffinity == AFFINITY_CORE_ANY))
        {
            return (taskQueueRemove(pQueue, currentTask, linkOf) == RET_SUCCESS) ? currentTask : NULL;
        }

        currentTask = nextTask;
    }

    return NULL;
}

static taskHandleType *taskQueuePeek(taskQueueType *pQueue,
                                     bool affinityCheck,
                                     taskQueueLinkAccessorType linkOf)
{
    taskHandleType *currentTask = NULL;

    if ((pQueue == NULL) || (linkOf == NULL) || taskQueueEmpty(pQueue))
    {
        return NULL;
    }

    if (!affinityCheck)
    {
        return pQueue->head;
    }

    currentTask = pQueue->head;
    while (currentTask != NULL)
    {
        if ((currentTask->coreAffinity == AFFINITY_CORE_ANY) ||
            (currentTask->coreAffinity == PORT_CORE_ID()))
        {
            return currentTask;
        }

        currentTask = taskQueueNextTask(pQueue, currentTask, linkOf);
    }

    return NULL;
}

taskQueueType *readyQueue(void)
{
    return &taskPool.readyQueue;
}

taskQueueType *blockedQueue(void)
{
    return &taskPool.blockedQueue;
}

taskQueueType *timeoutQueue(void)
{
    return &taskPool.timeoutQueue;
}

int readyQueueAdd(taskHandleType *pTask)
{
    return taskQueueAddPrioritySorted(readyQueue(), pTask, taskStateQueueLink);
}

int blockedQueueAdd(taskHandleType *pTask)
{
    taskQueueType *pBlockedQueue = blockedQueue();
    return taskQueueInsertBetween(pBlockedQueue, NULL, pBlockedQueue->head, pTask, taskStateQueueLink);
}

int waitQueueAdd(taskQueueType *pWaitQueue, taskHandleType *pTask)
{
    return taskQueueAddPrioritySorted(pWaitQueue, pTask, taskWaitQueueLink);
}

int timeoutQueueAdd(taskHandleType *pTask)
{
    return taskQueueAddDeadlineSorted(timeoutQueue(), pTask, taskTimeoutQueueLink);
}

int readyQueueRemove(taskHandleType *pTask)
{
    return taskQueueRemove(readyQueue(), pTask, taskStateQueueLink);
}

int blockedQueueRemove(taskHandleType *pTask)
{
    return taskQueueRemove(blockedQueue(), pTask, taskStateQueueLink);
}

int waitQueueRemove(taskHandleType *pTask)
{
    return taskQueueRemoveOwned(pTask, taskWaitQueueLink);
}

int timeoutQueueRemove(taskHandleType *pTask)
{
    return taskQueueRemove(timeoutQueue(), pTask, taskTimeoutQueueLink);
}

taskHandleType *stateQueueNext(taskQueueType *pStateQueue, taskHandleType *pTask)
{
    return taskQueueNextTask(pStateQueue, pTask, taskStateQueueLink);
}

taskHandleType *waitQueueNext(taskQueueType *pWaitQueue, taskHandleType *pTask)
{
    return taskQueueNextTask(pWaitQueue, pTask, taskWaitQueueLink);
}

taskHandleType *readyQueuePop(void)
{
    return taskQueuePop(readyQueue(), true, taskStateQueueLink);
}

taskHandleType *waitQueuePop(taskQueueType *pWaitQueue)
{
    return taskQueuePop(pWaitQueue, false, taskWaitQueueLink);
}

taskHandleType *readyQueuePeek(void)
{
    return taskQueuePeek(readyQueue(), true, taskStateQueueLink);
}

taskHandleType *waitQueuePeek(taskQueueType *pWaitQueue)
{
    return taskQueuePeek(pWaitQueue, false, taskWaitQueueLink);
}

taskHandleType *timeoutQueuePeek(void)
{
    return taskQueuePeek(timeoutQueue(), false, taskTimeoutQueueLink);
}
