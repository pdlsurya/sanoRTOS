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

#define MSG_QUEUE_DEFINE(msgQueueHandle, length, item_size) \
    uint8_t msgQueueHandle##Buffer[length * item_size];     \
    msgQueueHandleType msgQueueHandle = {                   \
        .producerWaitQueue = {0},                           \
        .consumerWaitQueue = {0},                           \
        .buffer = msgQueueHandle##Buffer,                   \
        .queueLength = length,                              \
        .itemSize = item_size,                              \
        .itemCount = 0,                                     \
        .readIndex = 0,                                     \
        .writeIndex = 0}

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

/**
 * @brief Check if message queue is full
 *
 * @param pQueueHandle
 * @return true
 * @return false
 */
static inline bool msgQueueFull(msgQueueHandleType *pQueueHandle)
{
    return pQueueHandle->itemCount == pQueueHandle->queueLength;
}

/**
 * @brief Check if message queue is empty
 *
 * @param pQueueHandle
 * @return true
 * @return false
 */
static inline bool msgQueueEmpty(msgQueueHandleType *pQueueHandle)
{
    return pQueueHandle->itemCount == 0;
}

bool msgQueueSend(msgQueueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks);

bool msgQueueReceive(msgQueueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks);

#endif