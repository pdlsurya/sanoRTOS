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

#ifndef __SANO_RTOS_MAILBOX_H
#define __SANO_RTOS_MAILBOX_H

#include <stdint.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MAILBOX_ANY_TASK NULL

/**
 * @brief Statically define and initialize a mailbox object.
 * @param _name Name of the mailbox object.
 */
#define MAILBOX_DEFINE(_name)       \
    mailboxHandleType _name = {     \
        .name = #_name,             \
        .senderWaitQueue = {0},     \
        .receiverWaitQueue = {0},   \
        .lock = 0}

    /**
     * @brief Mailbox message descriptor exchanged between sending and receiving tasks.
     */
    typedef struct mailboxMsg
    {
        uint32_t info;               ///< Application-defined mailbox metadata.
        uint32_t size;               ///< Message size, or receive buffer capacity on input.
        const void *pTxData;         ///< Sender data buffer. Ignored by mailboxReceive().
        taskHandleType *pTargetTask; ///< Receiver requested by the sender, or NULL for any.
        taskHandleType *pSourceTask; ///< Sender requested by the receiver, or NULL for any.
    } mailboxMsgType;

    /**
     * @brief Mailbox object used to match sending and receiving tasks.
     */
    typedef struct mailboxHandle
    {
        const char *name;               ///< Mailbox name.
        taskQueueType senderWaitQueue;  ///< Queue of tasks waiting to send a mailbox message.
        taskQueueType receiverWaitQueue; ///< Queue of tasks waiting to receive a mailbox message.
        atomic_t lock;                  ///< Spinlock protecting the mailbox object.
    } mailboxHandleType;

    /**
     * @brief Send a message through a mailbox.
     *
     * The caller blocks until a compatible receiver consumes the message or the timeout expires.
     * Mailboxes are thread-only objects and cannot be used from ISR context.
     *
     * @param pMailbox Pointer to mailbox handle.
     * @param pMsg Pointer to mailbox message descriptor.
     * @param waitTicks Maximum ticks to wait for a compatible receiver.
     * @return `RET_SUCCESS` on success, `RET_BUSY` if `waitTicks` is `TASK_NO_WAIT`
     *         and no compatible receiver is waiting, `RET_TIMEOUT` on timeout,
     *         error code otherwise.
     */
    int mailboxSend(mailboxHandleType *pMailbox, mailboxMsgType *pMsg, uint32_t waitTicks);

    /**
     * @brief Receive a message from a mailbox.
     *
     * The caller blocks until a compatible sender is available or the timeout expires.
     * Mailboxes are thread-only objects and cannot be used from ISR context.
     *
     * @param pMailbox Pointer to mailbox handle.
     * @param pMsg Pointer to mailbox message descriptor.
     * @param pBuffer Destination buffer receiving message data.
     * @param waitTicks Maximum ticks to wait for a compatible sender.
     * @return `RET_SUCCESS` on success, `RET_BUSY` if `waitTicks` is `TASK_NO_WAIT`
     *         and no compatible sender is waiting, `RET_TIMEOUT` on timeout,
     *         error code otherwise.
     */
    int mailboxReceive(mailboxHandleType *pMailbox, mailboxMsgType *pMsg, void *pBuffer, uint32_t waitTicks);

#ifdef __cplusplus
}
#endif

#endif
