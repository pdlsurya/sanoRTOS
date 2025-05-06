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
 * @brief Dynamically allocates and initializes a new task node.
 *
 * @param pTask Pointer to the task handle structure.
 * @return Pointer to the newly allocated task node.
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
 * @brief Removes the front (head) node from the given task queue.
 *
 * Frees the memory associated with the removed node.
 *
 * @param pTaskQueue Pointer to the task queue.
 */
static inline void taskQueueRemoveHead(taskQueueType *pTaskQueue)
{
    taskNodeType *temp = pTaskQueue->head->nextTaskNode;
    memFree(pTaskQueue->head);
    pTaskQueue->head = temp;
}

/**
 * @brief Removes a specific task from the task queue.
 *
 * If the task is at the front, it removes the head node. Otherwise, it traverses
 * the list and unlinks the node associated with the given task.
 *
 * @param pTaskQueue Pointer to the task queue.
 * @param pTask Pointer to the task handle to be removed.
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
 * @brief Inserts a task at the front of the queue without considering task priority.
 *
 * Useful when priority ordering is not required.
 *
 * @param pTaskQueue Pointer to the task queue.
 * @param pTask Pointer to the task to be inserted.
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
 * @brief Inserts a task into the queue in ascending order of priority.
 *
 * Lower numerical value indicates higher priority. The queue is kept sorted
 * to enable efficient retrieval of the highest-priority task.
 *
 * @param pTaskQueue Pointer to the task queue.
 * @param pTask Pointer to the task to be inserted.
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
        while (currentTaskNode->nextTaskNode &&
               currentTaskNode->nextTaskNode->pTask->priority <= pTask->priority)
        {
            currentTaskNode = currentTaskNode->nextTaskNode;
        }

        newTaskNode->nextTaskNode = currentTaskNode->nextTaskNode;
        currentTaskNode->nextTaskNode = newTaskNode;
    }
}

/**
 * @brief Retrieves and removes the highest-priority task that is eligible to run on this core.
 *
 * Scans the queue from front to back, checking each task's core affinity.
 *
 * @param pTaskQueue Pointer to the task queue.
 * @return Pointer to the task handle of the selected task, or NULL if none found.
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

/**
 * @brief Peeks the highest-priority task eligible to run on this core without removing it.
 *
 * Similar to taskQueueGet() but does not modify the queue.
 *
 * @param pTaskQueue Pointer to the task queue.
 * @return Pointer to the task handle, or NULL if no eligible task is found.
 */
taskHandleType *taskQueuePeek(taskQueueType *pTaskQueue)
{
    if (!taskQueueEmpty(pTaskQueue))
    {
        taskNodeType *currentTaskNode = pTaskQueue->head;
        while (currentTaskNode != NULL)
        {
            if (currentTaskNode->pTask->coreAffinity == PORT_CORE_ID() ||
                currentTaskNode->pTask->coreAffinity == AFFINITY_CORE_ANY)
            {
                return currentTaskNode->pTask;
            }

            currentTaskNode = currentTaskNode->nextTaskNode;
        }
    }
    return NULL;
}
