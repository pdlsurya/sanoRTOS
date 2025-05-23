/*
 * MIT License
 *
 * Copyright (c) 2024 Surya Poudel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>
#include <assert.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/messageQueue.h"
#include "sanoRTOS/mutex.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/scheduler.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"

/**
 * @brief Insert an item to the queue buffer
 *
 * @param pQueueHandle Pointer to msgQueueHandle struct.
 * @param pItem Pointer to the item
 */
static bool msgQueueBufferWrite(msgQueueHandleType *pQueueHandle, void *pItem)
{
    bool irqState = spinLock(&pQueueHandle->lock);

    bool contextSwitchRequired = false;

    taskHandleType *pConsumerTask = NULL;
    if (!msgQueueFull(pQueueHandle))
    {

        memcpy(&pQueueHandle->buffer[pQueueHandle->writeIndex], pItem, pQueueHandle->itemSize);
        pQueueHandle->writeIndex = (pQueueHandle->writeIndex + pQueueHandle->itemSize) % (pQueueHandle->queueLength * pQueueHandle->itemSize);
        pQueueHandle->itemCount++;

        // Get next waiting pConsumerTask task to unblock
    getNextConsumer:
        pConsumerTask = TASK_GET_FROM_WAIT_QUEUE(&pQueueHandle->consumerWaitQueue);
        if (pConsumerTask != NULL)
        {
            /*If task was suspended while waiting for Queue data, skipt the task and get another waiting task from the waitQueue*/
            if (pConsumerTask->status == TASK_STATUS_SUSPENDED)
            {
                goto getNextConsumer;
            }
            taskSetReady(pConsumerTask, MSG_QUEUE_DATA_AVAILABLE);

            taskHandleType *currentTask = taskGetCurrent();

            /*Perform context switch if  unblocked pConsumerTask task has equal or
             *higher priority[lower priority value] than that of current task */
            if (pConsumerTask->priority <= currentTask->priority)
            {
                contextSwitchRequired = true;
            }
        }

        spinUnlock(&pQueueHandle->lock, irqState);

        if (contextSwitchRequired)
        {
            taskYield();
        }
        return true;
    }
    else
    {
        spinUnlock(&pQueueHandle->lock, irqState);
        return false;
    }
}

/**
 * @brief  Get an item from the queue buffer
 *
 * @param pQueueHandle Pointer to msgQueueHandle struct.
 * @param pItem Pointer to the item
 */
static bool msgQueueBufferRead(msgQueueHandleType *pQueueHandle, void *pItem)
{

    bool irqState = spinLock(&pQueueHandle->lock);

    bool contextSwitchRequired = false;

    taskHandleType *pProducerTask = NULL;

    if (!msgQueueEmpty(pQueueHandle))
    {

        memcpy(pItem, &pQueueHandle->buffer[pQueueHandle->readIndex], pQueueHandle->itemSize);
        pQueueHandle->readIndex = (pQueueHandle->readIndex + pQueueHandle->itemSize) % (pQueueHandle->queueLength * pQueueHandle->itemSize);
        pQueueHandle->itemCount--;

        // Get next waiting pProducerTask task to unblock
    getNextProducer:
        pProducerTask = TASK_GET_FROM_WAIT_QUEUE(&pQueueHandle->producerWaitQueue);
        if (pProducerTask != NULL)
        {
            /*If task was suspended while waiting for Queue space, skip the task and get another waiting task from the wait Queue*/
            if (pProducerTask->status == TASK_STATUS_SUSPENDED)
            {
                goto getNextProducer;
            }
            taskSetReady(pProducerTask, MSG_QUEUE_SPACE_AVAILABE);

            taskHandleType *currentTask = taskGetCurrent();

            /*Perform context switch if unblocked pProducerTask task has equal or
             *higher priority[lower priority value] than that of current task */
            if (pProducerTask->priority <= currentTask->priority)
            {
                contextSwitchRequired = true;
            }
        }

        spinUnlock(&pQueueHandle->lock, irqState);

        if (contextSwitchRequired)
        {
            taskYield();
        }

        return true;
    }
    else
    {
        spinUnlock(&pQueueHandle->lock, irqState);

        return false;
    }
}

/**
 * @brief Send an item to the queue. If the queue if full, block the task for specified number of wait ticks.
 * If calling this function from an ISR, the parameter waitTicksshould be set to TASK_NO_WAIT.
 * @param pQueueHandle Pointer to queueHandle struct.
 * @param pItem Pointer to the item to be sent to the Queue.
 * @param waitTicks Number of ticks to wait if Queue is full.
 * @retval `RET_SUCCESS` if message sent successfully.
 * @retval `RET_FUL`L if Queue is full.
 * @retval `RET_TIMEOUT` if wait timeout occured.
 */
int msgQueueSend(msgQueueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks)
{
    assert(pQueueHandle != NULL);
    assert(pItem != NULL);

    int retCode;

    /*Write to msgQueue buffer if messageQueue is not full*/
retry:
    if (msgQueueBufferWrite(pQueueHandle, pItem))
    {
        retCode = RET_SUCCESS;
    }
    else if (waitTicks == TASK_NO_WAIT)
    {
        retCode = RET_FULL;
    }
    else
    {
        taskHandleType *currentTask = taskGetCurrent();

        bool irqState = spinLock(&pQueueHandle->lock);

        taskQueueAdd(&pQueueHandle->producerWaitQueue, currentTask);

        spinUnlock(&pQueueHandle->lock, irqState);

        // Block current task and  give CPU to other tasks while waiting for space to be available
        taskBlock(currentTask, WAIT_FOR_MSG_QUEUE_SPACE, waitTicks);

        if (currentTask->wakeupReason == MSG_QUEUE_SPACE_AVAILABE && msgQueueBufferWrite(pQueueHandle, pItem))
        {

            retCode = RET_SUCCESS;
        }
        else if (currentTask->wakeupReason == WAIT_TIMEOUT)
        {
            irqState = spinLock(&pQueueHandle->lock);

            /*Wait timed out,remove task from wait Queue.*/
            taskQueueRemove(&pQueueHandle->producerWaitQueue, currentTask);

            spinUnlock(&pQueueHandle->lock, irqState);

            retCode = RET_TIMEOUT;
        }
        /*Task might have been suspended while waiting for space to be available and later resumed.
          In this case, retry sending to the msgQueue again */
        else
        {
            goto retry;
        }
    }
    return retCode;
}

/**
 * @brief Receive an item from the queue. If the queue is empty, block the task for specified  number of wait ticks.
 * If calling this function from an ISR, the parameter waitTicks should be set to TASK_NO_WAIT.
 * @param pQueueHandle Pointer to queueHandle struct
 * @param pItem Pointer to the variable to be assigned the data received from the Queue.
 * @param waitTicks Number of ticks to wait if Queue is empty.
 * @retval `RET_SUCCESS` if message received successfully.
 * @retval `RET_EMPTY` if Queue is empty.
 * @retval `RET_TIMEOUT` if wait timeout occured.
 */
int msgQueueReceive(msgQueueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks)
{
    assert(pQueueHandle != NULL);
    assert(pItem != NULL);

    int retCode;

retry:
    if (msgQueueBufferRead(pQueueHandle, pItem))
    {
        retCode = RET_SUCCESS;
    }
    else if (waitTicks == TASK_NO_WAIT)
    {
        retCode = RET_EMPTY;
    }
    else
    {
        taskHandleType *currentTask = taskGetCurrent();

        bool irqState = spinLock(&pQueueHandle->lock);

        taskQueueAdd(&pQueueHandle->consumerWaitQueue, currentTask);

        spinUnlock(&pQueueHandle->lock, irqState);

        // Block current task and give CPU to other tasks while waiting for data to be available
        taskBlock(currentTask, WAIT_FOR_MSG_QUEUE_DATA, waitTicks);

        if (currentTask->wakeupReason == MSG_QUEUE_DATA_AVAILABLE && msgQueueBufferRead(pQueueHandle, pItem))
        {
            retCode = RET_SUCCESS;
        }
        else if (currentTask->wakeupReason == WAIT_TIMEOUT)
        {
            irqState = spinLock(&pQueueHandle->lock);

            spinLock(&pQueueHandle->lock);

            /*Wait timed out,remove task from wait Queue.*/
            taskQueueRemove(&pQueueHandle->consumerWaitQueue, currentTask);

            spinUnlock(&pQueueHandle->lock, irqState);

            retCode = RET_TIMEOUT;
        }
        /*Task might have been suspended while waiting for data to be available and later resumed.
        In this case, retry receiving from the msgQueue again */
        else
        {
            goto retry;
        }
    }
    return retCode;
}
