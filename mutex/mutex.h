/**
 * @file mutex.h
 * @author Surya Poudel
 * @brief Mutex APIs
 * @version 0.1
 * @date 2024-04-09
 *
 * @copyright Copyright (c) 2024 Surya Poudel
 *
 */

#ifndef __SANO_RTOS_MUTEX_H
#define __SANO_RTOS_MUTEX_H

#include <stdint.h>
#include <stdbool.h>
#include "task/task.h"
#include "taskQueue/taskQueue.h"
#include "osConfig.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Statically define and initialize a mutex.
 * @param name Name of the mutex.
 */
#define MUTEX_DEFINE(name)          \
    mutexHandleType name = {        \
        .waitQueue = {0},           \
        .ownerTask = NULL,          \
        .ownerDefaultPriority = -1, \
        .locked = false}

    typedef struct
    {
        taskQueueType waitQueue;
        taskHandleType *ownerTask;
        int16_t ownerDefaultPriority;
        bool locked;

    } mutexHandleType;

    int mutexLock(mutexHandleType *pMutex, uint32_t waitTicks);

    int mutexUnlock(mutexHandleType *pMutex);

#ifdef __cplusplus
}
#endif

#endif