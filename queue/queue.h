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

#ifndef __SANO_RTOS_MSG_QUEUE_H
#define __SANO_RTOS_MSG_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/utils.h"

typedef struct
{
    taskQueueType producerWaitQueue;
    taskQueueType consumerWaitQueue;
    uint8_t *buffer;
    uint32_t queueLength;
    uint32_t itemSize;
    uint32_t itemCount;
    uint32_t readIndex;
    uint32_t writeIndex;
} msgQueueHandleType;
bool msgQueueFull(msgQueueHandleType *pQueueHandle);

bool msgQueueEmpty(msgQueueHandleType *pQueueHandle);

bool msgQueueCreate(msgQueueHandleType *pQueueHandle, void *buffer, uint32_t queueLength, uint32_t itemSize);

bool msgQueueSend(msgQueueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks);

bool msgQueueReceive(msgQueueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks);

#endif