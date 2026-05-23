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

#ifndef __SANO_RTOS_TASK_QUEUE_H
#define __SANO_RTOS_TASK_QUEUE_H

#include <stddef.h>
#include <stdint.h>
#include "sanoRTOS/config.h"
#include "sanoRTOS/port.h"
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/spinLock.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Forward declaration of taskHandleType.
     */
    typedef struct taskHandle taskHandleType;

    /**
     * @brief Task queue structure used to manage a list of tasks (e.g., for ready, waiting, or blocked states).
     */
    typedef struct
    {
        taskHandleType *head; ///< Pointer to the head of the task queue linked list.
        taskHandleType *tail; ///< Pointer to the tail of the task queue linked list.
        atomicType *pLock;    ///< Optional object lock protecting this wait queue.
    } taskQueueType;

#if CONFIG_READY_QUEUE_PRIORITY_MULTIQ
    /**
     * @brief Bitmap-backed ready queue organized as FIFO queues per priority and affinity class.
     */
    typedef struct
    {
        taskQueueType queues[CONFIG_TASK_PRIORITY_LEVELS][(PORT_CORE_COUNT == 1U) ? 1U : (PORT_CORE_COUNT + 1U)];
        uint32_t bitmaps[(PORT_CORE_COUNT == 1U) ? 1U : (PORT_CORE_COUNT + 1U)];
    } readyQueueType;
#else
    /**
     * @brief Priority-sorted ready queue backend.
     */
    typedef taskQueueType readyQueueType;
#endif

    /**
     * @brief Intrusive doubly linked queue link stored inside each task.
     */
    typedef struct
    {
        taskHandleType *pPrevTask;  ///< Previous task in the queue.
        taskHandleType *pNextTask;  ///< Next task in the queue.
        taskQueueType *pOwnerQueue; ///< Queue currently owning this link, or NULL if not queued.
    } taskQueueLinkType;

#define TASK_WAIT_QUEUE_INITIALIZER \
    {                               \
        .head = NULL,               \
        .tail = NULL,               \
        .pLock = NULL}

    /**
     * @brief Get the global ready queue.
     *
     * @return Pointer to the scheduler ready queue.
     */
    readyQueueType *readyQueue(void);

    /**
     * @brief Get the global blocked queue.
     *
     * @return Pointer to the scheduler blocked queue.
     */
    taskQueueType *blockedQueue(void);

    /**
     * @brief Get the global timeout queue.
     *
     * @return Pointer to the scheduler timeout queue.
     */
    taskQueueType *timeoutQueue(void);

    /**
     * @brief Pop the next eligible task from a ready queue.
     *
     * @return Task handle pointer, or `NULL` if none available.
     */
    taskHandleType *readyQueuePop(void);

    /**
     * @brief Pop the next task from an object wait queue.
     *
     * @param pWaitQueue Pointer to wait queue.
     * @return Task handle pointer, or `NULL` if none available.
     */
    taskHandleType *waitQueuePop(taskQueueType *pWaitQueue);

    /**
     * @brief Peek next eligible task from a ready queue without removing it.
     *
     * @return Task handle pointer, or `NULL` if none available.
     */
    taskHandleType *readyQueuePeek(void);

    /**
     * @brief Peek next task from an object wait queue without removing it.
     *
     * @param pWaitQueue Pointer to wait queue.
     * @return Task handle pointer, or `NULL` if none available.
     */
    taskHandleType *waitQueuePeek(taskQueueType *pWaitQueue);

    /**
     * @brief Add a task to the configured ready queue backend.
     *
     * @param pTask Pointer to task handle.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    int readyQueueAdd(taskHandleType *pTask);

    /**
     * @brief Add a task to the blocked queue.
     *
     * Blocked tasks are inserted at the front because the blocked queue is not
     * priority-sorted.
     *
     * @param pTask Pointer to task handle.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    int blockedQueueAdd(taskHandleType *pTask);

    /**
     * @brief Add a task to a wait queue sorted by priority.
     *
     * @param pWaitQueue Pointer to wait queue.
     * @param pTask Pointer to task handle.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    int waitQueueAdd(taskQueueType *pWaitQueue, taskHandleType *pTask);

    /**
     * @brief Add a task to the timeout queue sorted by absolute deadline.
     *
     * @param pTask Pointer to task handle.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    int timeoutQueueAdd(taskHandleType *pTask);

    /**
     * @brief Remove a task from the ready queue.
     *
     * @param pTask Pointer to task handle.
     * @return `RET_SUCCESS` on success, `RET_NOTASK` if not queued, error code otherwise.
     */
    int readyQueueRemove(taskHandleType *pTask);

    /**
     * @brief Remove a task from the blocked queue.
     *
     * @param pTask Pointer to task handle.
     * @return `RET_SUCCESS` on success, `RET_NOTASK` if not queued, error code otherwise.
     */
    int blockedQueueRemove(taskHandleType *pTask);

    /**
     * @brief Remove a task from whichever wait queue currently owns its wait link.
     *
     * @param pTask Pointer to task handle.
     * @return `RET_SUCCESS` on success, `RET_NOTASK` if not queued, error code otherwise.
     */
    int waitQueueRemove(taskHandleType *pTask);

    /**
     * @brief Remove a task from the timeout queue.
     *
     * @param pTask Pointer to task handle.
     * @return `RET_SUCCESS` on success, `RET_NOTASK` if not queued, error code otherwise.
     */
    int timeoutQueueRemove(taskHandleType *pTask);

    /**
     * @brief Get the next task in a state queue.
     *
     * @param pStateQueue Pointer to state queue.
     * @param pTask Current task in the queue.
     * @return Next task handle pointer, or `NULL` if this was the last task or arguments are invalid.
     */
    taskHandleType *stateQueueNext(taskQueueType *pStateQueue, taskHandleType *pTask);

    /**
     * @brief Get the next task in a wait queue.
     *
     * @param pWaitQueue Pointer to wait queue.
     * @param pTask Current task in the queue.
     * @return Next task handle pointer, or `NULL` if this was the last task or arguments are invalid.
     */
    taskHandleType *waitQueueNext(taskQueueType *pWaitQueue, taskHandleType *pTask);

    /**
     * @brief Peek the next task due in the timeout queue.
     *
     * @return Task handle pointer, or `NULL` if none available.
     */
    taskHandleType *timeoutQueuePeek(void);

    /**
     * @brief Check whether taskQueue is empty.
     *
     * @param pTaskQueue Pointer to task queue.
     * @retval `true` if queue is empty.
     * @retval `false` otherwise.
     */
    static inline __attribute__((always_inline)) bool taskQueueEmpty(taskQueueType *pTaskQueue)
    {
        return (pTaskQueue == NULL) || (pTaskQueue->head == NULL);
    }

#ifdef __cplusplus
}
#endif

#endif
