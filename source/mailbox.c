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
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/scheduler.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"
#include "sanoRTOS/mailbox.h"

static inline void mailboxTaskStateReset(taskHandleType *pTask)
{
    if (pTask == NULL)
    {
        return;
    }

    /* Clear any mailbox state saved while the task was waiting on a mailbox object */
    memset(&pTask->mailboxState, 0, sizeof(pTask->mailboxState));
}

static inline bool mailboxTasksMatch(taskHandleType *pSenderTask, taskHandleType *pReceiverTask)
{
    return (pSenderTask != NULL) &&
           (pReceiverTask != NULL) &&
           ((pSenderTask->mailboxState.msg.pTargetTask == MAILBOX_ANY_TASK) ||
            (pSenderTask->mailboxState.msg.pTargetTask == pReceiverTask)) &&
           ((pReceiverTask->mailboxState.msg.pSourceTask == MAILBOX_ANY_TASK) ||
            (pReceiverTask->mailboxState.msg.pSourceTask == pSenderTask));
}

static int mailboxTransferData(taskHandleType *pSenderTask, taskHandleType *pReceiverTask)
{
    if ((pSenderTask == NULL) || (pReceiverTask == NULL))
    {
        return RET_INVAL;
    }

    /* Transfer at most the smaller of sender message size and receiver buffer capacity */
    uint32_t transferSize = pSenderTask->mailboxState.msg.size;
    if (pReceiverTask->mailboxState.msg.size < transferSize)
    {
        transferSize = pReceiverTask->mailboxState.msg.size;
    }

    if ((transferSize > 0U) &&
        ((pSenderTask->mailboxState.msg.pTxData == NULL) || (pReceiverTask->mailboxState.pRxBuffer == NULL)))
    {
        return RET_INVAL;
    }

    if (transferSize > 0U)
    {
        memcpy(pReceiverTask->mailboxState.pRxBuffer, pSenderTask->mailboxState.msg.pTxData, transferSize);
    }

    /* Publish the matched peer and actual transfer size back to both tasks */
    pSenderTask->mailboxState.msg.size = transferSize;
    pSenderTask->mailboxState.msg.pTargetTask = pReceiverTask;

    pReceiverTask->mailboxState.msg.info = pSenderTask->mailboxState.msg.info;
    pReceiverTask->mailboxState.msg.size = transferSize;
    pReceiverTask->mailboxState.msg.pSourceTask = pSenderTask;

    return RET_SUCCESS;
}

static int mailboxWakeMatchedTask(taskQueueType *pWaitQueue, taskHandleType *pTask, bool *pContextSwitchRequired)
{
    if ((pWaitQueue == NULL) || (pTask == NULL) || (pContextSwitchRequired == NULL))
    {
        return RET_INVAL;
    }

    /* Remove the matched task from the mailbox wait queue before making it READY */
    int retCode = taskQueueRemove(pWaitQueue, pTask);
    if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
    {
        return retCode;
    }

    retCode = taskSetReady(pTask, MAILBOX_TRANSFER_DONE);
    if (retCode != RET_SUCCESS)
    {
        return retCode;
    }

    if (taskCanPreemptCurrentCore(pTask))
    {
        *pContextSwitchRequired = true;
    }

    return RET_SUCCESS;
}

int mailboxSend(mailboxHandleType *pMailbox, mailboxMsgType *pMsg, uint32_t waitTicks)
{
    if ((pMailbox == NULL) || (pMsg == NULL))
    {
        return RET_INVAL;
    }
    if (portIsInISRContext())
    {
        return RET_INVAL;
    }
    if ((pMsg->size > 0U) && (pMsg->pTxData == NULL))
    {
        return RET_INVAL;
    }

    taskHandleType *currentTask = taskGetCurrent();
    if (currentTask == NULL)
    {
        return RET_INVAL;
    }

    bool contextSwitchRequired;
    bool irqState;
    int retCode;
    taskHandleType *pReceiverTask;
    taskNodeType *currentTaskNode;

    /* Save the caller's send request in the task's mailbox wait state */
    currentTask->mailboxState.msg = *pMsg;
    currentTask->mailboxState.pRxBuffer = NULL;

retry:
    contextSwitchRequired = false;
    irqState = spinLock(&pMailbox->lock);

    pReceiverTask = NULL;
    currentTaskNode = pMailbox->receiverWaitQueue.head;

    /* Check waiting receivers for one whose source/target filters match this sender */
    while (currentTaskNode != NULL)
    {
        taskHandleType *pTask = currentTaskNode->pTask;

        if ((pTask->status == TASK_STATUS_BLOCKED) &&
            (pTask->blockedReason == WAIT_FOR_MAILBOX_RECEIVE) &&
            mailboxTasksMatch(currentTask, pTask))
        {
            pReceiverTask = pTask;
            break;
        }

        currentTaskNode = currentTaskNode->nextTaskNode;
    }

    /* Perform the mailbox transfer immediately when a compatible receiver is already waiting */
    if (pReceiverTask != NULL)
    {
        retCode = mailboxTransferData(currentTask, pReceiverTask);
        if (retCode != RET_SUCCESS)
        {
            mailboxTaskStateReset(currentTask);
            spinUnlock(&pMailbox->lock, irqState);
            return retCode;
        }

        retCode = mailboxWakeMatchedTask(&pMailbox->receiverWaitQueue, pReceiverTask, &contextSwitchRequired);
        if (retCode != RET_SUCCESS)
        {
            mailboxTaskStateReset(currentTask);
            spinUnlock(&pMailbox->lock, irqState);
            return retCode;
        }

        *pMsg = currentTask->mailboxState.msg;
        mailboxTaskStateReset(currentTask);
        retCode = RET_SUCCESS;
    }
    else if (waitTicks == TASK_NO_WAIT)
    {
        /* No compatible receiver available and caller requested no-wait */
        mailboxTaskStateReset(currentTask);
        retCode = RET_BUSY;
    }
    else
    {
        /* Add current task to the sender wait queue before blocking */
        retCode = taskQueueAdd(&pMailbox->senderWaitQueue, currentTask);
        if (retCode != RET_SUCCESS)
        {
            mailboxTaskStateReset(currentTask);
            spinUnlock(&pMailbox->lock, irqState);
            return retCode;
        }

        spinUnlock(&pMailbox->lock, irqState);

        /* Block current task and give CPU to other tasks while waiting for a compatible receiver. */
        retCode = taskBlock(currentTask, WAIT_FOR_MAILBOX_SEND, waitTicks);
        if (retCode != RET_SUCCESS)
        {
            irqState = spinLock(&pMailbox->lock);
            (void)taskQueueRemove(&pMailbox->senderWaitQueue, currentTask);
            mailboxTaskStateReset(currentTask);
            spinUnlock(&pMailbox->lock, irqState);
            return retCode;
        }

        irqState = spinLock(&pMailbox->lock);

        /*Task has been woken up either due to mailbox transfer completion or wait timeout */
        if (currentTask->wakeupReason == MAILBOX_TRANSFER_DONE)
        {
            *pMsg = currentTask->mailboxState.msg;
            mailboxTaskStateReset(currentTask);
            retCode = RET_SUCCESS;
        }
        else if (currentTask->wakeupReason == WAIT_TIMEOUT)
        {
            retCode = taskQueueRemove(&pMailbox->senderWaitQueue, currentTask);
            mailboxTaskStateReset(currentTask);

            if ((retCode == RET_SUCCESS) || (retCode == RET_NOTASK))
            {
                retCode = RET_TIMEOUT;
            }
        }
        else
        {
            /*Task might have been suspended while waiting on mailbox send and later resumed.
              In this case, retry sending to the mailbox again */
            retCode = taskQueueRemove(&pMailbox->senderWaitQueue, currentTask);
            spinUnlock(&pMailbox->lock, irqState);

            if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
            {
                mailboxTaskStateReset(currentTask);
                return retCode;
            }

            goto retry;
        }
    }

    spinUnlock(&pMailbox->lock, irqState);

    if (contextSwitchRequired)
    {
        taskYield();
    }

    return retCode;
}

int mailboxReceive(mailboxHandleType *pMailbox, mailboxMsgType *pMsg, void *pBuffer, uint32_t waitTicks)
{
    if ((pMailbox == NULL) || (pMsg == NULL))
    {
        return RET_INVAL;
    }
    if (portIsInISRContext())
    {
        return RET_INVAL;
    }
    if ((pMsg->size > 0U) && (pBuffer == NULL))
    {
        return RET_INVAL;
    }

    taskHandleType *currentTask = taskGetCurrent();
    if (currentTask == NULL)
    {
        return RET_INVAL;
    }

    bool contextSwitchRequired;
    bool irqState;
    int retCode;
    taskHandleType *pSenderTask;
    taskNodeType *currentTaskNode;

    /* Save the caller's receive request and destination buffer in the task's mailbox wait state */
    currentTask->mailboxState.msg = *pMsg;
    currentTask->mailboxState.msg.pTxData = NULL;
    currentTask->mailboxState.pRxBuffer = pBuffer;

retry:
    contextSwitchRequired = false;
    irqState = spinLock(&pMailbox->lock);

    pSenderTask = NULL;
    currentTaskNode = pMailbox->senderWaitQueue.head;

    /* Check waiting senders for one whose source/target filters match this receiver */
    while (currentTaskNode != NULL)
    {
        taskHandleType *pTask = currentTaskNode->pTask;

        if ((pTask->status == TASK_STATUS_BLOCKED) &&
            (pTask->blockedReason == WAIT_FOR_MAILBOX_SEND) &&
            mailboxTasksMatch(pTask, currentTask))
        {
            pSenderTask = pTask;
            break;
        }

        currentTaskNode = currentTaskNode->nextTaskNode;
    }

    /* Perform the mailbox transfer immediately when a compatible sender is already waiting */
    if (pSenderTask != NULL)
    {
        retCode = mailboxTransferData(pSenderTask, currentTask);
        if (retCode != RET_SUCCESS)
        {
            mailboxTaskStateReset(currentTask);
            spinUnlock(&pMailbox->lock, irqState);
            return retCode;
        }

        retCode = mailboxWakeMatchedTask(&pMailbox->senderWaitQueue, pSenderTask, &contextSwitchRequired);
        if (retCode != RET_SUCCESS)
        {
            mailboxTaskStateReset(currentTask);
            spinUnlock(&pMailbox->lock, irqState);
            return retCode;
        }

        *pMsg = currentTask->mailboxState.msg;
        mailboxTaskStateReset(currentTask);
        retCode = RET_SUCCESS;
    }
    else if (waitTicks == TASK_NO_WAIT)
    {
        /* No compatible sender available and caller requested no-wait */
        mailboxTaskStateReset(currentTask);
        retCode = RET_BUSY;
    }
    else
    {
        /* Add current task to the receiver wait queue before blocking */
        retCode = taskQueueAdd(&pMailbox->receiverWaitQueue, currentTask);
        if (retCode != RET_SUCCESS)
        {
            mailboxTaskStateReset(currentTask);
            spinUnlock(&pMailbox->lock, irqState);
            return retCode;
        }

        spinUnlock(&pMailbox->lock, irqState);

        /* Block current task and give CPU to other tasks while waiting for a compatible sender. */
        retCode = taskBlock(currentTask, WAIT_FOR_MAILBOX_RECEIVE, waitTicks);
        if (retCode != RET_SUCCESS)
        {
            irqState = spinLock(&pMailbox->lock);
            (void)taskQueueRemove(&pMailbox->receiverWaitQueue, currentTask);
            mailboxTaskStateReset(currentTask);
            spinUnlock(&pMailbox->lock, irqState);
            return retCode;
        }

        irqState = spinLock(&pMailbox->lock);

        /*Task has been woken up either due to mailbox transfer completion or wait timeout */
        if (currentTask->wakeupReason == MAILBOX_TRANSFER_DONE)
        {
            *pMsg = currentTask->mailboxState.msg;
            mailboxTaskStateReset(currentTask);
            retCode = RET_SUCCESS;
        }
        else if (currentTask->wakeupReason == WAIT_TIMEOUT)
        {
            retCode = taskQueueRemove(&pMailbox->receiverWaitQueue, currentTask);
            mailboxTaskStateReset(currentTask);

            if ((retCode == RET_SUCCESS) || (retCode == RET_NOTASK))
            {
                retCode = RET_TIMEOUT;
            }
        }
        else
        {
            /*Task might have been suspended while waiting on mailbox receive and later resumed.
              In this case, retry receiving from the mailbox again */
            retCode = taskQueueRemove(&pMailbox->receiverWaitQueue, currentTask);
            spinUnlock(&pMailbox->lock, irqState);

            if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
            {
                mailboxTaskStateReset(currentTask);
                return retCode;
            }

            goto retry;
        }
    }

    spinUnlock(&pMailbox->lock, irqState);

    if (contextSwitchRequired)
    {
        taskYield();
    }

    return retCode;
}
