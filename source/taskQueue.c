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
#include <assert.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/config.h"
#include "sanoRTOS//task.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/mem.h"

static inline taskNodeType *newNode(taskHandleType *pTask)
{
    taskNodeType *newTaskNode = (taskNodeType *)memAlloc(sizeof(taskNodeType));
    assert(newTaskNode != NULL);

    newTaskNode->pTask = pTask;
    newTaskNode->nextTaskNode = NULL;

    return newTaskNode;
}

static inline void taskQueueRemoveHead(taskQueueType *pTaskQueue)
{
    taskNodeType *temp = pTaskQueue->head->nextTaskNode;
    memFree(pTaskQueue->head);
    pTaskQueue->head = temp;
}

void taskQueueRemove(taskQueueType *pTaskQueue, taskHandleType *pTask)
{
    assert(pTaskQueue != NULL);
    assert(pTask != NULL);

    if (pTask == pTaskQueue->head->pTask)
    {
        taskQueueRemoveHead(pTaskQueue);
    }
    else
    {
        taskNodeType *currentTaskNode = pTaskQueue->head;
        while (currentTaskNode->nextTaskNode->pTask != pTask)
        {
            currentTaskNode = currentTaskNode->nextTaskNode;
        }

        taskNodeType *temp = currentTaskNode->nextTaskNode->nextTaskNode;
        memFree(currentTaskNode->nextTaskNode);
        currentTaskNode->nextTaskNode = temp;
    }
}

void taskQueueAddToFront(taskQueueType *pTaskQueue, taskHandleType *pTask)
{
    assert(pTaskQueue != NULL);
    assert(pTask != NULL);

    taskNodeType *newTaskNode = newNode(pTask);
    newTaskNode->nextTaskNode = pTaskQueue->head;
    pTaskQueue->head = newTaskNode;
}

void taskQueueAdd(taskQueueType *pTaskQueue, taskHandleType *pTask)
{
    assert(pTaskQueue != NULL);
    assert(pTask != NULL);

    taskNodeType *newTaskNode = newNode(pTask);

    if (taskQueueEmpty(pTaskQueue))
    {
        pTaskQueue->head = newTaskNode;
    }
    else if (pTaskQueue->head->pTask->priority > pTask->priority)
    {
        newTaskNode->nextTaskNode = pTaskQueue->head;
        pTaskQueue->head = newTaskNode;
    }
    else
    {
        taskNodeType *currentTaskNode = pTaskQueue->head;
        while (currentTaskNode->nextTaskNode &&
               currentTaskNode->nextTaskNode->pTask->priority <= pTask->priority)
        {
            currentTaskNode = currentTaskNode->nextTaskNode;
        }

        newTaskNode->nextTaskNode = currentTaskNode->nextTaskNode;
        currentTaskNode->nextTaskNode = newTaskNode;
    }
}

taskHandleType *taskQueueGet(taskQueueType *pTaskQueue, bool affinityCheck)
{
    assert(pTaskQueue != NULL);
    taskHandleType *pTask;

    if (!taskQueueEmpty(pTaskQueue))
    {
        taskNodeType *currentTaskNode = pTaskQueue->head;

        if (!affinityCheck)
        {
            pTask = currentTaskNode->pTask;
            taskQueueRemove(pTaskQueue, pTask);
            return pTask;
        }

        /* Check each task in the queue for a matching core affinity */
        while (currentTaskNode != NULL)
        {
            if (currentTaskNode->pTask->coreAffinity == PORT_CORE_ID() ||
                currentTaskNode->pTask->coreAffinity == AFFINITY_CORE_ANY)
            {
                pTask = currentTaskNode->pTask;
                taskQueueRemove(pTaskQueue, pTask);
                return pTask;
            }
            currentTaskNode = currentTaskNode->nextTaskNode;
        }
    }

    return NULL;
}

taskHandleType *taskQueuePeek(taskQueueType *pTaskQueue, bool affinityCheck)
{
    if (!taskQueueEmpty(pTaskQueue))
    {
        taskNodeType *currentTaskNode = pTaskQueue->head;

        if (!affinityCheck)
        {
            return currentTaskNode->pTask;
        }

        while (currentTaskNode != NULL)
        {
            if (currentTaskNode->pTask->coreAffinity == AFFINITY_CORE_ANY ||
                currentTaskNode->pTask->coreAffinity == PORT_CORE_ID())
            {
                return currentTaskNode->pTask;
            }

            currentTaskNode = currentTaskNode->nextTaskNode;
        }
    }
    return NULL;
}
