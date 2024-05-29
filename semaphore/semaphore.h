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
#include "utils/utils.h"

#define SEMAPHORE_DEFINE(semHandle, initialCount, maxCnt) \
    semaphoreHandleType semHandle = {                     \
        .waitQueue = {0},                                 \
        .count = initialCount,                            \
        .maxCount = maxCnt}

typedef struct
{
    taskQueueType waitQueue;
    uint8_t count;
    uint8_t maxCount;
} semaphoreHandleType;

bool semaphoreTake(semaphoreHandleType *pSem, uint32_t waitTicks);

bool semaphoreGive(semaphoreHandleType *pSem);

#endif