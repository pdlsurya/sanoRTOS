/**
 * @file taskQueue.c
 * @author Surya Poudel
 * @brief Priority Task Queue implementation 
 * @version 0.1
 * @date 2024-05-08
 *
 * @copyright Copyright (c) 2024 Surya Poudel
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "retCodes.h"
#include "osConfig.h"
#include "task/task.h"
#include "taskQueue.h"
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