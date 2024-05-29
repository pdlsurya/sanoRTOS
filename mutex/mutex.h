/**
 * @file mutex.h
 * @author Surya Poudel
 * @brief
 * @version 0.1
 * @date 2024-04-09
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef __SANO_RTOS_MUTEX_H
#define __SANO_RTOS_MUTEX_H

#include <stdint.h>
#include <stdbool.h>
#include "task/task.h"
#include "utils/utils.h"
#include "osConfig.h"

#define MUTEX_DEFINE(mutexHandle)   \
    mutexHandleType mutexHandle = { \
        .waitQueue = {0},           \
        .ownerTask = NULL,          \
        .ownerDefaultPriority = -1, \
        .locked = false}

typedef struct
{
    taskQueueType waitQueue;
    volatile taskHandleType *ownerTask;
    int16_t ownerDefaultPriority;
    bool locked;

} mutexHandleType;

bool mutexLock(mutexHandleType *pMutex, uint32_t waitTicks);

bool mutexUnlock(mutexHandleType *pMutex);

#endif