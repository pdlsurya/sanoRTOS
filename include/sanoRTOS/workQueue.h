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

#ifndef __SANO_RTOS_WORK_QUEUE_H
#define __SANO_RTOS_WORK_QUEUE_H

#include <stdbool.h>
#include <stdint.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/timer.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/spinLock.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void (*workHandlerType)(void *pArg);

    /**
     * @brief Work item queued for execution by a work queue.
     */
    typedef struct workItem
    {
        const char *name;          ///< Work item name used for debugging.
        workHandlerType handler;   ///< Function executed by the work queue worker task.
        void *pArg;                ///< Application argument passed to the work handler.
        struct workItem *pNextWork; ///< Pointer to the next queued work item.
        bool pending;              ///< Non-zero while the work item is queued.
    } workItemType;

    /**
     * @brief Work queue object used to process work items in FIFO order.
     */
    typedef struct
    {
        const char *name;           ///< Work queue name.
        workItemType *head;         ///< Head of the queued work item list.
        workItemType *tail;         ///< Tail of the queued work item list.
        taskHandleType *pWorkerTask; ///< Worker task that processes queued work items.
        atomic_t lock;              ///< Spinlock protecting the work queue.
    } workQueueHandleType;

    /**
     * @brief Delayed work item backed by a one-shot timer.
     */
    typedef struct delayedWork
    {
        workItemType work;               ///< Work item submitted to the target work queue on timeout.
        timerNodeType timer;             ///< One-shot timer used to delay work submission.
        workQueueHandleType *pWorkQueue; ///< Target work queue used when the timer expires.
        bool scheduled;                  ///< Non-zero while delay timeout is still pending.
    } delayedWorkType;

    /**
     * @brief Generic worker task entry used by statically defined work queues.
     *
     * @param pArgs Pointer to work queue handle.
     */
    void workQueueTask(void *pArgs);

    /**
     * @brief Internal timeout handler used by delayed work items.
     *
     * @param pArg Pointer to delayed work item.
     */
    void delayedWorkTimeoutHandler(void *pArg);

/**
 * @brief Statically define and initialize a work item.
 * @param _name Name of the work item.
 * @param _handler Work handler function.
 * @param _arg Application argument passed to the work handler.
 */
#define WORK_DEFINE(_name, _handler, _arg) \
    workItemType _name = {                 \
        .name = #_name,                    \
        .handler = _handler,               \
        .pArg = _arg,                      \
        .pNextWork = NULL,                 \
        .pending = false}

/**
 * @brief Statically define and initialize a work queue and its worker task.
 * @param _name Name of the work queue.
 * @param stackSize Worker task stack size in bytes.
 * @param taskPriority Worker task priority.
 * @param affinity Worker task core affinity.
 */
#define WORK_QUEUE_DEFINE(_name, stackSize, taskPriority, affinity)                      \
    extern workQueueHandleType _name;                                                    \
    TASK_DEFINE(_name##WorkerTask, stackSize, workQueueTask, &_name, taskPriority, affinity); \
    workQueueHandleType _name = {                                                        \
        .name = #_name,                                                                  \
        .head = NULL,                                                                    \
        .tail = NULL,                                                                    \
        .pWorkerTask = &_name##WorkerTask,                                               \
        .lock = 0}

/**
 * @brief Statically define and initialize a delayed work item.
 * @param _name Name of the delayed work item.
 * @param _handler Work handler function.
 * @param _arg Application argument passed to the work handler.
 */
#define DELAYED_WORK_DEFINE(_name, _handler, _arg)          \
    delayedWorkType _name = {                               \
        .work = {                                           \
            .name = #_name,                                 \
            .handler = _handler,                            \
            .pArg = _arg,                                   \
            .pNextWork = NULL,                              \
            .pending = false},                              \
        .timer = {                                          \
            .name = #_name,                                 \
            .timeoutHandler = delayedWorkTimeoutHandler,    \
            .pArg = &_name,                                 \
            .intervalTicks = 0,                             \
            .ticksToExpire = 0,                             \
            .nextNode = NULL,                               \
            .mode = TIMER_MODE_SINGLE_SHOT,                 \
            .isRunning = false},                            \
        .pWorkQueue = NULL,                                 \
        .scheduled = false}

    /**
     * @brief Initialize a work item at runtime.
     *
     * @param pWork Pointer to work item.
     * @param name Work item name.
     * @param handler Work handler function.
     * @param pArg Application argument passed to the work handler.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    static inline __attribute__((always_inline)) int workInit(workItemType *pWork, const char *name,
                                                              workHandlerType handler, void *pArg)
    {
        if ((pWork == NULL) || (handler == NULL))
        {
            return RET_INVAL;
        }

        pWork->name = name;
        pWork->handler = handler;
        pWork->pArg = pArg;
        pWork->pNextWork = NULL;
        pWork->pending = false;

        return RET_SUCCESS;
    }

    /**
     * @brief Initialize a delayed work item at runtime.
     *
     * @param pDelayedWork Pointer to delayed work item.
     * @param name Delayed work item name.
     * @param handler Work handler function.
     * @param pArg Application argument passed to the work handler.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    static inline __attribute__((always_inline)) int delayedWorkInit(delayedWorkType *pDelayedWork,
                                                                     const char *name,
                                                                     workHandlerType handler,
                                                                     void *pArg)
    {
        if (pDelayedWork == NULL)
        {
            return RET_INVAL;
        }

        int retCode = workInit(&pDelayedWork->work, name, handler, pArg);
        if (retCode != RET_SUCCESS)
        {
            return retCode;
        }

        pDelayedWork->timer.name = name;
        pDelayedWork->timer.timeoutHandler = delayedWorkTimeoutHandler;
        pDelayedWork->timer.pArg = pDelayedWork;
        pDelayedWork->timer.intervalTicks = 0U;
        pDelayedWork->timer.ticksToExpire = 0U;
        pDelayedWork->timer.nextNode = NULL;
        pDelayedWork->timer.mode = TIMER_MODE_SINGLE_SHOT;
        pDelayedWork->timer.isRunning = false;
        pDelayedWork->pWorkQueue = NULL;
        pDelayedWork->scheduled = false;

        return RET_SUCCESS;
    }

    /**
     * @brief Check whether a work item is currently queued.
     *
     * @param pWork Pointer to work item.
     * @retval `true` if work item is pending.
     * @retval `false` otherwise.
     */
    static inline __attribute__((always_inline)) bool workPending(workItemType *pWork)
    {
        return (pWork != NULL) && pWork->pending;
    }

    /**
     * @brief Check whether delayed work is still waiting for its timeout or is already queued.
     *
     * @param pDelayedWork Pointer to delayed work item.
     * @retval `true` if delayed work is scheduled or queued.
     * @retval `false` otherwise.
     */
    static inline __attribute__((always_inline)) bool delayedWorkPending(delayedWorkType *pDelayedWork)
    {
        return (pDelayedWork != NULL) && (pDelayedWork->scheduled || workPending(&pDelayedWork->work));
    }

    /**
     * @brief Start the worker task for a statically defined work queue.
     *
     * @param pWorkQueue Pointer to work queue handle.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    int workQueueStart(workQueueHandleType *pWorkQueue);

    /**
     * @brief Submit a work item to a work queue.
     *
     * Submitting the same work item again while it is already queued returns `RET_BUSY`.
     * This API can be called from task or ISR context.
     *
     * @param pWorkQueue Pointer to work queue handle.
     * @param pWork Pointer to work item.
     * @return `RET_SUCCESS` on success, `RET_BUSY` if the work item is already pending,
     *         error code otherwise.
     */
    int workSubmit(workQueueHandleType *pWorkQueue, workItemType *pWork);

    /**
     * @brief Schedule a delayed work item on a work queue.
     *
     * If `delayTicks` is zero, the work item is submitted immediately.
     * This first implementation rejects scheduling while the delayed work is
     * already waiting for timeout or already queued in the work queue.
     *
     * @param pWorkQueue Pointer to work queue handle.
     * @param pDelayedWork Pointer to delayed work item.
     * @param delayTicks Delay in RTOS ticks before the work item is submitted.
     * @return `RET_SUCCESS` on success, `RET_BUSY` if delayed work is already pending,
     *         error code otherwise.
     */
    int delayedWorkSchedule(workQueueHandleType *pWorkQueue, delayedWorkType *pDelayedWork,
                            uint32_t delayTicks);

    /**
     * @brief Cancel a delayed work item before it is submitted to the work queue.
     *
     * This first implementation only cancels delayed work while its timer is
     * still active. Once timeout handling has begun or the work item is already
     * queued, cancellation returns `RET_BUSY`.
     *
     * @param pDelayedWork Pointer to delayed work item.
     * @return `RET_SUCCESS` on success, `RET_BUSY` if cancellation is too late,
     *         `RET_NOTACTIVE` if delayed work is not pending, error code otherwise.
     */
    int delayedWorkCancel(delayedWorkType *pDelayedWork);

#ifdef __cplusplus
}
#endif

#endif
