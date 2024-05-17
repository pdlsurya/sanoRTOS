/**
 * @file queue.h
 * @author Surya Poudel
 * @brief FIFO Queue implementation for inter-task communication
 * @version 0.1
 * @date 2024-04-15
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef __SANO_RTOS_QUEUE_H
#define __SANO_RTOS_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/utils.h"

typedef struct
{
    waitQueueType producerWaitQueue;
    waitQueueType consumerWaitQueue;
    uint8_t *buffer;
    uint32_t queueLength;
    uint32_t itemSize;
    uint32_t itemCount;
    uint32_t readIndex;
    uint32_t writeIndex;
} queueHandleType;
bool isQueueFull(queueHandleType *pQueueHandle);

bool isQueueEmpty(queueHandleType *pQueueHandle);

bool queueCreate(queueHandleType *pQueueHandle, void *buffer, uint32_t queueLength, uint32_t itemSize);

bool queueSend(queueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks);

bool queueReceive(queueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks);

#endif