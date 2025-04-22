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
/**
 * @brief Dynamically allocate memory for new task Node.
 *
 * @param pTask Pointer to taskHandle struct
 * @return Pointer to new task node
 */
static inline taskNodeType *newNode(taskHandleType *pTask)
{
    taskNodeType *newTaskNode = (taskNodeType *)memAlloc(sizeof(taskNodeType));

    assert(newTaskNode != NULL);

    newTaskNode->pTask = pTask;
    newTaskNode->nextTaskNode = NULL;

    return newTaskNode;
}

/**
 * @brief Remove head node from Queue
 *
 * @param pTaskQueue
 */
static inline void taskQueueRemoveHead(taskQueueType *pTaskQueue)
{
    taskNodeType *temp = pTaskQueue->head->nextTaskNode;

    memFree(pTaskQueue->head);

    pTaskQueue->head = temp;
}

/**
 * @brief Remove task from Queue
 *
 * @param pTaskQueue
 * @param pTask
 */
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

/**
 * @brief Add task to front of the Queue without sorting
 *
 * @param pTaskQueue Pointer to the taskQueue struct.
 * @param pTask  Pointer to the taskHandle struct
 */
void taskQueueAddToFront(taskQueueType *pTaskQueue, taskHandleType *pTask)
{
    assert(pTaskQueue != NULL);
    assert(pTask != NULL);

    taskNodeType *newTaskNode = newNode(pTask);

    newTaskNode->nextTaskNode = pTaskQueue->head;

    pTaskQueue->head = newTaskNode;
}

/**
 * @brief Add task to Queue and  sort tasks in ascending order of
 * their priority
 * @param pTaskQueue
 * @param pTask
 */
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
        while (currentTaskNode->nextTaskNode && currentTaskNode->nextTaskNode->pTask->priority <= pTask->priority)
        {
            currentTaskNode = currentTaskNode->nextTaskNode;
        }

        newTaskNode->nextTaskNode = currentTaskNode->nextTaskNode;

        currentTaskNode->nextTaskNode = newTaskNode;
    }
}

/**
 * @brief Get the highest priority task from the Queue. This corresponds to the
 * front task node in the Queue.
 * @param pTaskQueue Pointer to taskQueue struct
 * @retval Next highest priority task if exists
 * @retval NULL if Queue is empty
 */
taskHandleType *taskQueueGet(taskQueueType *pTaskQueue)
{
    assert(pTaskQueue != NULL);
    taskHandleType *pTask;

    if (!taskQueueEmpty(pTaskQueue))
    {
        taskNodeType *currentTaskNode = pTaskQueue->head;

        while (currentTaskNode != NULL)
        {
            if (currentTaskNode->pTask->coreAffinity == PORT_CORE_ID() || currentTaskNode->pTask->coreAffinity == AFFINITY_CORE_ANY)
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

/**
 * @brief Get task corresponding to front node from the task queue without removing the node
 *
 * @param pTaskQueue Pointer to the taskQueue struct
 * @return Pointer to taskHandle struct corresponding to front node
 */
taskHandleType *taskQueuePeek(taskQueueType *pTaskQueue)
{
    if (!taskQueueEmpty(pTaskQueue))
    {
        taskNodeType *currentTaskNode = pTaskQueue->head;
        while (currentTaskNode != NULL)
        {
            if (currentTaskNode->pTask->coreAffinity == PORT_CORE_ID() || currentTaskNode->pTask->coreAffinity == AFFINITY_CORE_ANY)
            {
                return currentTaskNode->pTask;
            }

            currentTaskNode = currentTaskNode->nextTaskNode;
        }
    }
    return NULL;
}
