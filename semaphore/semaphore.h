/**
 * @file semaphore.h
 * @author Surya Poudel
 * @brief
 * @version 0.1
 * @date 2024-04-09
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef __SANO_RTOS_SEMAPHORE_H
#define __SANO_RTOS_SEMAPHORE_H

#include <stdint.h>
#include <stdbool.h>
#include "osConfig.h"
#include "task/task.h"
#include "taskQueue/taskQueue.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Statically define and initialize a semaphore.
 * @param name Name of the semaphore.
 * @param initialCount Initial semaphore count.
 * @param maxCnt Maximum semaphore count.
 */
#define SEMAPHORE_DEFINE(name, initialCount, maxCnt) \
    semaphoreHandleType name = {                     \
        .waitQueue = {0},                                 \
        .count = initialCount,                            \
        .maxCount = maxCnt}

    typedef struct
    {
        taskQueueType waitQueue;
        uint8_t count;
        uint8_t maxCount;
    } semaphoreHandleType;

    int semaphoreTake(semaphoreHandleType *pSem, uint32_t waitTicks);

    int semaphoreGive(semaphoreHandleType *pSem);

#ifdef __cplusplus
}
#endif

#endif