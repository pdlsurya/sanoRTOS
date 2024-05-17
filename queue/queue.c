/**
 * @file queue.c
 * @author Surya Poudel
 * @brief FIFO Queue implementation for inter-task communication.
 * @version 0.1
 * @date 2024-04-15
 *
 * @copyright Copyright (c) 2024
 *
 */
#include <string.h>
#include "queue.h"
#include "task/task.h"
#include "scheduler/scheduler.h"
#include "utils/utils.h"

/**
 * @brief Insert an item to the queue buffer
 *
 * @param pQueueHandle
 * @param pItem
 */
static void queueBufferWrite(queueHandleType *pQueueHandle, void *pItem)
{
    memcpy(&pQueueHandle->buffer[pQueueHandle->writeIndex], pItem, pQueueHandle->itemSize);
    pQueueHandle->writeIndex = (pQueueHandle->writeIndex + pQueueHandle->itemSize) % (pQueueHandle->queueLength * pQueueHandle->itemSize);
    pQueueHandle->itemCount++;

    // Get next waiting consumer task to unblock
    taskHandleType *consumer = waitQueueGet(&pQueueHandle->consumerWaitQueue);
    if (consumer)
        taskSetReady(consumer, QUEUE_DATA_AVAILABLE);
}

/**
 * @brief  Get an item from the queue buffer
 *
 * @param pQueueHandle
 * @param pItem
 */
static void queueBufferRead(queueHandleType *pQueueHandle, void *pItem)
{
    memcpy(pItem, &pQueueHandle->buffer[pQueueHandle->readIndex], pQueueHandle->itemSize);
    pQueueHandle->readIndex = (pQueueHandle->readIndex + pQueueHandle->itemSize) % (pQueueHandle->queueLength * pQueueHandle->itemSize);
    pQueueHandle->itemCount--;

    // Get next waiting producer task to unblock
    taskHandleType *producer = waitQueueGet(&pQueueHandle->producerWaitQueue);
    if (producer)
        taskSetReady(producer, QUEUE_SPACE_AVAILABE);
}

/**
 * @brief Check if queue is full
 *
 * @param pQueueHandle
 * @return true
 * @return false
 */
bool isQueueFull(queueHandleType *pQueueHandle)
{
    return pQueueHandle->itemCount == pQueueHandle->queueLength;
}

/**
 * @brief Check if queue is empty
 *
 * @param pQueueHandle
 * @return true
 * @return false
 */
bool isQueueEmpty(queueHandleType *pQueueHandle)
{
    return pQueueHandle->itemCount == 0;
}

/**
 * @brief Create a queue with given queue parameters.
 *
 * @param pQueueHandle
 * @param buffer
 * @param queueLength
 * @param itemSize
 * @return true
 * @return false
 */
bool queueCreate(queueHandleType *pQueueHandle, void *buffer, uint32_t queueLength, uint32_t itemSize)
{
    if (pQueueHandle)
    {
        // Initialize wait queue
        waitQueueInit(&pQueueHandle->producerWaitQueue);
        waitQueueInit(&pQueueHandle->consumerWaitQueue);
        pQueueHandle->buffer = buffer;
        pQueueHandle->queueLength = queueLength;
        pQueueHandle->itemSize = itemSize;
        pQueueHandle->readIndex = 0;
        pQueueHandle->writeIndex = 0;
        return true;
    }
    return false;
}

/**
 * @brief Send an item to the queue. If the queue if full, block the for specified
 * wait ticks.
 * @param pQueueHandle Pointer to queueHandle struct
 * @param pItem Pointer to the item to be sent to the Queue
 * @param waitTicks Number of ticks to wait if space is not available/Queue is full
 * @return true on Success
 * @return false on Fail
 */
bool queueSend(queueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks)
{
    if (pQueueHandle)
    {

        if (pQueueHandle->producerWaitQueue.count == 0 && !isQueueFull(pQueueHandle))
        {
            queueBufferWrite(pQueueHandle, pItem);

            return true;
        }
        else if (waitTicks == TASK_NO_WAIT && isQueueFull(pQueueHandle))
            return false;
        else
        {
            taskHandleType *currentTask = taskPool.tasks[taskPool.currentTaskId];

            waitQueuePut(&pQueueHandle->producerWaitQueue, currentTask);

            // Block current task and  give CPU to other tasks while waiting for space to be available
            taskBlock(currentTask, WAIT_FOR_QUEUE_SPACE, waitTicks);

            if (currentTask->wakeupReason == QUEUE_SPACE_AVAILABE && !isQueueFull(pQueueHandle))
            {
                queueBufferWrite(pQueueHandle, pItem);

                return true;
            }
        }
    }
    return false;
}

/**
 * @brief Receive an item from the queue. If the queue is empty, block the task for specified wait ticks.
 *
 * @param pQueueHandle Pointer to queueHandle struct
 * @param pItem Pointe to the variable to be assigned the data received from the Queue.
 * @param waitTicks Number of ticks to wait if Queue is empty
 * @return true on Success
 * @return false on Fail
 */
bool queueReceive(queueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks)
{
    if (pQueueHandle)
    {

        if (pQueueHandle->consumerWaitQueue.count == 0 && !isQueueEmpty(pQueueHandle))
        {
            queueBufferRead(pQueueHandle, pItem);
            return true;
        }
        else if (waitTicks == TASK_NO_WAIT && isQueueEmpty(pQueueHandle))
            return false;
        else
        {
            taskHandleType *currentTask = taskPool.tasks[taskPool.currentTaskId];

            waitQueuePut(&pQueueHandle->consumerWaitQueue, currentTask);

            // Block current task and give CPU to other tasks while waiting for data to be available
            taskBlock(currentTask, WAIT_FOR_QUEUE_DATA, waitTicks);

            if (currentTask->wakeupReason == QUEUE_DATA_AVAILABLE && !isQueueEmpty(pQueueHandle))
            {
                queueBufferRead(pQueueHandle, pItem);

                return true;
            }
        }
    }
    return false;
}