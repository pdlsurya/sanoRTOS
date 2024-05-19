/**
 * @file utils.h
 * @author Surya Poudel
 * @brief Utility functions
 * @version 0.1
 * @date 2024-05-08
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef __SANO_RTOS_UTILS_H
#define __SANO_RTOS_UTILS_H

#include "osConfig.h"

typedef struct taskHandle taskHandleType;

typedef struct taskNode
{
    taskHandleType *pTask;
    struct taskNode *nextTaskNode;
} taskNodeType;

typedef struct
{
    taskNodeType *head;
} taskQueueType;

taskHandleType *taskQueueGet(taskQueueType *pTaskQueue);

void taskQueueInsert(taskQueueType *pTaskQueue, taskHandleType *pTask);

void taskQueueRemove(taskQueueType *pTaskQueue, taskHandleType *pTask);

static inline bool taskQueueEmpty(taskQueueType *pTaskQueue)
{
    return pTaskQueue->head == NULL;
}

#endif