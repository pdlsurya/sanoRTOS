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

typedef struct
{
    waitQueueType waitQueue;
    uint8_t count;
    uint8_t maxCount;
} semaphoreHandleType;

void semaphoreInit(semaphoreHandleType *pSem, uint8_t initialCount, uint8_t maxCount);

bool semaphoreTake(semaphoreHandleType *pSem, uint32_t waitTicks);

bool semaphoreGive(semaphoreHandleType *pSem);

#endif