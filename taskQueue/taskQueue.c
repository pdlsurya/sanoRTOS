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
#include "retCodes.h"
#include "osConfig.h"
#include "task/task.h"
#include "taskQueue.h"
/**
 * @brief Dynamically allocate memory for new task Node.
 *
 * @param pTask Pointer to taskHandle struct
 * @return Poiter to new task node
 */
static inline taskNodeType *newNode(taskHandleType *pTask)
{
    taskNodeType *newTaskNode = (taskNodeType *)malloc(sizeof(taskNodeType));
    newTaskNode->pTask = pTask;
    newTaskNode->nextTaskNode = NULL;
    return newTaskNode;
}

/**
 * @brief Add task to front of the Queue without sorting
 *
 * @param pTaskQueue Pointer to the taskQueue struct.
 * @param pTask  Pointer to the taskHandle struct
 * @retval RET_SUCCESS if task added successfully
 * @retval RET_INVAL if invalid arguments passed
 */
int taskQueueAddToFront(taskQueueType *pTaskQueue, taskHandleType *pTask)
{
    if (pTaskQueue != NULL && pTask != NULL)
    {
        taskNodeType *newTaskNode = newNode(pTask);

        newTaskNode->nextTaskNode = pTaskQueue->head;

        pTaskQueue->head = newTaskNode;
        return RET_SUCCESS;
    }
    return RET_INVAL;
}

/**
 * @brief Add task to Queue and  sort tasks in ascending order of
 * their priority
 * @param pTaskQueue
 * @param pTask
 * @retval RET_SUCCESS if task added to the Queue successfully
 * @retval RET_INVAL if invalid argument passed
 */
int taskQueueAdd(taskQueueType *pTaskQueue, taskHandleType *pTask)
{
    if (pTaskQueue != NULL && pTask != NULL)
    {
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
        return RET_SUCCESS;
    }
    return RET_INVAL;
}

/**
 * @brief Get the highest priority task from the Queue. This corresponds to the
 * front task node in the Queue.
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

/**
 * @brief Remove head node from Queue
 *
 * @param pTaskQueue
 * @retval RET_SUCCESS if removed successfully
 * @retval RET_INVAL if invalid argument passed
 */
static inline int taskQueueRemoveHead(taskQueueType *pTaskQueue)
{
    if (pTaskQueue != NULL)
    {
        taskNodeType *temp = pTaskQueue->head->nextTaskNode;

        free(pTaskQueue->head);

        pTaskQueue->head = temp;

        return RET_SUCCESS;
    }
    return RET_INVAL;
}

/**
 * @brief Remove task from Queue
 *
 * @param pTaskQueue
 * @param pTask
 * @retval RET_SUCCESS if task removed successfully
 * @retval RET_INVAL if invalid argument passed
 */
int taskQueueRemove(taskQueueType *pTaskQueue, taskHandleType *pTask)
{
    if (pTaskQueue != NULL && pTask != NULL)
    {

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
        return RET_SUCCESS;
    }
    return RET_INVAL;
}