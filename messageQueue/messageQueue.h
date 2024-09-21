/**
 * @file queue.h
 * @author Surya Poudel
 * @brief FIFO Message Queue implementation for inter-task communication
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
#include "mutex/mutex.h"
#include "taskQueue/taskQueue.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Statically define and initialize a message queue. The message queue internally uses a ring buffer,
 * which is (length * item_size) bytes long, to store message items.The message queue operates
 * in First In First Out(FIFO) manner.
 * @param name Name of the message queue.
 * @param length Maximum number of message items the message queue can hold.
 * @param item_size Size of a message item in bytes.
 */
#define MSG_QUEUE_DEFINE(name, length, item_size) \
    uint8_t name##Buffer[length * item_size];     \
    msgQueueHandleType name = {                   \
        .producerWaitQueue = {0},                 \
        .consumerWaitQueue = {0},                 \
        .mutex = {.ownerDefaultPriority = -1},    \
        .buffer = name##Buffer,                   \
        .queueLength = length,                    \
        .itemSize = item_size,                    \
        .itemCount = 0,                           \
        .readIndex = 0,                           \
        .writeIndex = 0}

    typedef struct
    {
        taskQueueType producerWaitQueue;
        taskQueueType consumerWaitQueue;
        mutexHandleType mutex;
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
     * @retval true if msgQueue is full
     * @retval false otherwise
     */
    static inline bool msgQueueFull(msgQueueHandleType *pQueueHandle)
    {
        return pQueueHandle->itemCount == pQueueHandle->queueLength;
    }

    /**
     * @brief Check if message queue is empty
     *
     * @param pQueueHandle
     * @retval true if msgQueue is empty
     * @return false otherwise
     */
    static inline bool msgQueueEmpty(msgQueueHandleType *pQueueHandle)
    {
        return pQueueHandle->itemCount == 0;
    }

    int msgQueueSend(msgQueueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks);

    int msgQueueReceive(msgQueueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks);

#ifdef __cplusplus
}
#endif

#endif