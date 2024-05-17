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
#include "task/task.h"

#define WAIT_QUEUE_MAX_SIZE (MAX_TASKS_COUNT - 3)

typedef struct
{
    taskHandleType *waitingTasks[WAIT_QUEUE_MAX_SIZE];
    uint8_t count;
} waitQueueType;

taskHandleType *waitQueueGet(waitQueueType *pWaitQueue);

bool waitQueuePut(waitQueueType *pWaitQueue, taskHandleType *pTask);

void waitQueueInit(waitQueueType *pWaitQueue);

int sortCompareFunction(const void *pTask1, const void *pTask2);

int getTaskIndex(taskHandleType **array, uint8_t len, volatile taskHandleType *pTask);

#endif