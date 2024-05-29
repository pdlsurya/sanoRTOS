/**
 * @file utils.c
 * @author Surya Poudel
 * @brief Utility functions.
 * @version 0.1
 * @date 2024-05-08
 *
 * @copyright Copyright (c) 2024
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include "osConfig.h"
#include "task/task.h"
#include "utils.h"

/**
 * @brief Put waiting task to taskQueue
 *
 * @param pTaskQueue
 * @param pTask
 */
void taskQueueInsert(taskQueueType *pTaskQueue, taskHandleType *pTask)
{

    taskNodeType *newTaskNode = (taskNodeType *)malloc(sizeof(taskNodeType));
    newTaskNode->pTask = pTask;
    newTaskNode->nextTaskNode = NULL;

    if (taskQueueEmpty(pTaskQueue))
    {
        pTaskQueue->head = newTaskNode;
    }
    else if (pTaskQueue->head->pTask->priority > pTask->priority)
    {
        taskNodeType *tempHead = pTaskQueue->head;
        pTaskQueue->head = newTaskNode;
        pTaskQueue->head->nextTaskNode = tempHead;
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
 * @brief Get the highest priority task from the taskQueue
 *
 * @param pTaskQueue Pointer to taskQueue struct
 * @return Next highest priority task if Queue is not empty, or NULL otherwise.
 */
taskHandleType *taskQueueGet(taskQueueType *ptaskQueue)
{
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

static inline void taskQueueRemoveHead(taskQueueType *pTaskQueue)
{
    taskNodeType *temp = pTaskQueue->head->nextTaskNode;

    free(pTaskQueue->head);

    pTaskQueue->head = temp;
}

void taskQueueRemove(taskQueueType *pTaskQueue, taskHandleType *pTask)
{
    if (pTask == pTaskQueue->head->pTask)
        taskQueueRemoveHead(pTaskQueue);

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