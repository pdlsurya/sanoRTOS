/**
 * @file queue.c
 * @author Surya Poudel
 * @brief FIFO message Queue implementation for inter-task communication.
 * @version 0.1
 * @date 2024-04-15
 *
 * @copyright Copyright (c) 2024
 *
 */
#include <string.h>
#include "messageQueue.h"
#include "task/task.h"
#include "scheduler/scheduler.h"
#include "taskQueue/taskQueue.h"

/**
 * @brief Insert an item to the queue buffer
 *
 * @param pQueueHandle
 * @param pItem
 */
static void msgQueueBufferWrite(msgQueueHandleType *pQueueHandle, void *pItem)
{
    memcpy(&pQueueHandle->buffer[pQueueHandle->writeIndex], pItem, pQueueHandle->itemSize);
    pQueueHandle->writeIndex = (pQueueHandle->writeIndex + pQueueHandle->itemSize) % (pQueueHandle->queueLength * pQueueHandle->itemSize);
    pQueueHandle->itemCount++;

    // Get next waiting consumer task to unblock
    taskHandleType *consumer = taskQueueGet(&pQueueHandle->consumerWaitQueue);
    if (consumer)
        taskSetReady(consumer, MSG_QUEUE_DATA_AVAILABLE);
}

/**
 * @brief  Get an item from the queue buffer
 *
 * @param pQueueHandle
 * @param pItem
 */
static void msgQueueBufferRead(msgQueueHandleType *pQueueHandle, void *pItem)
{
    memcpy(pItem, &pQueueHandle->buffer[pQueueHandle->readIndex], pQueueHandle->itemSize);
    pQueueHandle->readIndex = (pQueueHandle->readIndex + pQueueHandle->itemSize) % (pQueueHandle->queueLength * pQueueHandle->itemSize);
    pQueueHandle->itemCount--;

    // Get next waiting producer task to unblock
    taskHandleType *producer = taskQueueGet(&pQueueHandle->producerWaitQueue);
    if (producer)
        taskSetReady(producer, MSG_QUEUE_SPACE_AVAILABE);
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
bool msgQueueSend(msgQueueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks)
{
    if (pQueueHandle)
    {

        if (taskQueueEmpty(&pQueueHandle->producerWaitQueue) && !msgQueueFull(pQueueHandle))
        {
            msgQueueBufferWrite(pQueueHandle, pItem);

            return true;
        }
        else if (waitTicks == TASK_NO_WAIT && msgQueueFull(pQueueHandle))
            return false;
        else
        {
            taskHandleType *currentTask = taskPool.currentTask;

            taskQueueAdd(&pQueueHandle->producerWaitQueue, currentTask);

            // Block current task and  give CPU to other tasks while waiting for space to be available
            taskBlock(currentTask, WAIT_FOR_MSG_QUEUE_SPACE, waitTicks);

            if (currentTask->wakeupReason == MSG_QUEUE_SPACE_AVAILABE && !msgQueueFull(pQueueHandle))
            {
                msgQueueBufferWrite(pQueueHandle, pItem);

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
bool msgQueueReceive(msgQueueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks)
{
    if (pQueueHandle)
    {

        if (taskQueueEmpty(&pQueueHandle->consumerWaitQueue) && !msgQueueEmpty(pQueueHandle))
        {
            msgQueueBufferRead(pQueueHandle, pItem);
            return true;
        }
        else if (waitTicks == TASK_NO_WAIT && msgQueueEmpty(pQueueHandle))
            return false;
        else
        {
            taskHandleType *currentTask = taskPool.currentTask;

            taskQueueAdd(&pQueueHandle->consumerWaitQueue, currentTask);

            // Block current task and give CPU to other tasks while waiting for data to be available
            taskBlock(currentTask, WAIT_FOR_MSG_QUEUE_DATA, waitTicks);

            if (currentTask->wakeupReason == MSG_QUEUE_DATA_AVAILABLE && !msgQueueEmpty(pQueueHandle))
            {
                msgQueueBufferRead(pQueueHandle, pItem);

                return true;
            }
        }
    }
    return false;
}