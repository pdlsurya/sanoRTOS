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
#include "sanoRTOS/config.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/memSlab.h"

MEM_SLAB_DEFINE(taskQueueNodeSlab, sizeof(taskNodeType), CONFIG_TASK_QUEUE_NODE_SLAB_BLOCKS);

static inline taskNodeType *newNode(taskHandleType *pTask)
{
    if (pTask == NULL)
    {
        return NULL;
    }

    taskNodeType *newTaskNode = NULL;
    if (memSlabAlloc(&taskQueueNodeSlab, (void **)&newTaskNode, TASK_NO_WAIT) != RET_SUCCESS)
    {
        return NULL;
    }

    newTaskNode->pTask = pTask;
    newTaskNode->nextTaskNode = NULL;

    return newTaskNode;
}

static inline int taskQueueRemoveHead(taskQueueType *pTaskQueue)
{
    if ((pTaskQueue == NULL) || (pTaskQueue->head == NULL))
    {
        return RET_INVAL;
    }

    taskNodeType *pNode = pTaskQueue->head;
    taskNodeType *temp = pNode->nextTaskNode;
    pTaskQueue->head = temp;

    return memSlabFree(&taskQueueNodeSlab, pNode);
}

int taskQueueRemove(taskQueueType *pTaskQueue, taskHandleType *pTask)
{
    if ((pTaskQueue == NULL) || (pTask == NULL))
    {
        return RET_INVAL;
    }

    if (pTaskQueue->head == NULL)
    {
        return RET_NOTASK;
    }

    if (pTask == pTaskQueue->head->pTask)
    {
        return taskQueueRemoveHead(pTaskQueue);
    }
    else
    {
        taskNodeType *currentTaskNode = pTaskQueue->head;
        while ((currentTaskNode->nextTaskNode != NULL) && (currentTaskNode->nextTaskNode->pTask != pTask))
        {
            currentTaskNode = currentTaskNode->nextTaskNode;
        }

        if (currentTaskNode->nextTaskNode == NULL)
        {
            return RET_NOTASK;
        }

        taskNodeType *pNode = currentTaskNode->nextTaskNode;
        taskNodeType *temp = pNode->nextTaskNode;
        currentTaskNode->nextTaskNode = temp;

        return memSlabFree(&taskQueueNodeSlab, pNode);
    }
}

int taskQueueAddToFront(taskQueueType *pTaskQueue, taskHandleType *pTask)
{
    if ((pTaskQueue == NULL) || (pTask == NULL))
    {
        return RET_INVAL;
    }

    taskNodeType *newTaskNode = newNode(pTask);
    if (newTaskNode == NULL)
    {
        return RET_NOMEM;
    }

    newTaskNode->nextTaskNode = pTaskQueue->head;
    pTaskQueue->head = newTaskNode;

    return RET_SUCCESS;
}

int taskQueueAdd(taskQueueType *pTaskQueue, taskHandleType *pTask)
{
    if ((pTaskQueue == NULL) || (pTask == NULL))
    {
        return RET_INVAL;
    }

    taskNodeType *newTaskNode = newNode(pTask);
    if (newTaskNode == NULL)
    {
        return RET_NOMEM;
    }

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

    return RET_SUCCESS;
}

taskHandleType *taskQueueGet(taskQueueType *pTaskQueue, bool affinityCheck)
{
    if (pTaskQueue == NULL)
    {
        return NULL;
    }

    taskHandleType *pTask;

    if (!taskQueueEmpty(pTaskQueue))
    {
        taskNodeType *currentTaskNode = pTaskQueue->head;

        if (!affinityCheck)
        {
            pTask = currentTaskNode->pTask;
            if (taskQueueRemove(pTaskQueue, pTask) == RET_SUCCESS)
            {
                return pTask;
            }
            return NULL;
        }

        /* Check each task in the queue for a matching core affinity */
        while (currentTaskNode != NULL)
        {
            if (currentTaskNode->pTask->coreAffinity == PORT_CORE_ID() ||
                currentTaskNode->pTask->coreAffinity == AFFINITY_CORE_ANY)
            {
                pTask = currentTaskNode->pTask;
                if (taskQueueRemove(pTaskQueue, pTask) == RET_SUCCESS)
                {
                    return pTask;
                }
                return NULL;
            }
            currentTaskNode = currentTaskNode->nextTaskNode;
        }
    }

    return NULL;
}

taskHandleType *taskQueuePeek(taskQueueType *pTaskQueue, bool affinityCheck)
{
    if (pTaskQueue == NULL)
    {
        return NULL;
    }

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
