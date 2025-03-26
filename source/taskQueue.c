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
#include <stdlib.h>
#include <assert.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/config.h"
#include "sanoRTOS//task.h"
#include "sanoRTOS/taskQueue.h"
/**
 * @brief Dynamically allocate memory for new task Node.
 *
 * @param pTask Pointer to taskHandle struct
 * @return Pointer to new task node
 */
static inline taskNodeType *newNode(taskHandleType *pTask)
{
    taskNodeType *newTaskNode = (taskNodeType *)malloc(sizeof(taskNodeType));

    assert(newTaskNode != NULL);

    newTaskNode->pTask = pTask;
    newTaskNode->nextTaskNode = NULL;

    return newTaskNode;
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
taskHandleType *taskQueueGet(taskQueueType *ptaskQueue)
{
    assert(ptaskQueue != NULL);

    if (!taskQueueEmpty(ptaskQueue))
    {
        taskHandleType *pTask = ptaskQueue->head->pTask;

        taskNodeType *temp = ptaskQueue->head->nextTaskNode;

        free(ptaskQueue->head);

        ptaskQueue->head = temp;

        return pTask;
    }

    return NULL;
}

/**
 * @brief Remove head node from Queue
 *
 * @param pTaskQueue
 */
static inline void taskQueueRemoveHead(taskQueueType *pTaskQueue)
{
    taskNodeType *temp = pTaskQueue->head->nextTaskNode;

    free(pTaskQueue->head);

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
            currentTaskNode = currentTaskNode->nextTaskNode;

        taskNodeType *temp = currentTaskNode->nextTaskNode->nextTaskNode;

        free(currentTaskNode->nextTaskNode);

        currentTaskNode->nextTaskNode = temp;
    }
}