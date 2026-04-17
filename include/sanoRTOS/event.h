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

#ifndef __SANO_RTOS_EVENT_H
#define __SANO_RTOS_EVENT_H

#include <stdint.h>
#include <stdbool.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Statically define and initialize an event object.
 * @param _name Name of the event object.
 */
#define EVENT_DEFINE(_name)         \
    eventHandleType _name = {       \
        .name = #_name,             \
        .waitQueue = {0},           \
        .events = 0,                \
        .lock = 0}

    /**
     * @brief Event object used to signal one or more bit-based events to waiting tasks.
     */
    typedef struct
    {
        const char *name;        ///< Name of the event object.
        taskQueueType waitQueue; ///< Queue of tasks waiting for event bits.
        uint32_t events;         ///< Current event-bit state.
        atomic_t lock;           ///< Spinlock protecting the event object.
    } eventHandleType;

    /**
     * @brief Set one or more bits in an event object.
     *
     * Tasks waiting on matching bits are made ready automatically.
     *
     * @param pEvent Pointer to event handle.
     * @param events Bit mask to set.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    int eventSet(eventHandleType *pEvent, uint32_t events);

    /**
     * @brief Clear one or more bits in an event object.
     *
     * @param pEvent Pointer to event handle.
     * @param events Bit mask to clear.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    int eventClear(eventHandleType *pEvent, uint32_t events);

    /**
     * @brief Get the current bits stored in an event object.
     *
     * @param pEvent Pointer to event handle.
     * @param pEvents Output pointer receiving the current event bits.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    int eventGet(eventHandleType *pEvent, uint32_t *pEvents);

    /**
     * @brief Wait until any bit in `events` becomes set.
     *
     * @param pEvent Pointer to event handle.
     * @param events Bit mask to wait for.
     * @param clearOnExit If `true`, clear the matched bits before returning successfully.
     * @param pMatchedEvents Output pointer receiving the matching bits.
     * @param waitTicks Maximum ticks to wait (`TASK_MAX_WAIT` for infinite wait).
     * @return `RET_SUCCESS` if any requested bit matched, `RET_TIMEOUT` on timeout,
     *         `RET_BUSY` if `waitTicks` is `TASK_NO_WAIT` and no bits matched, error code otherwise.
     */
    int eventWaitAny(eventHandleType *pEvent, uint32_t events,
                     bool clearOnExit, uint32_t *pMatchedEvents, uint32_t waitTicks);

    /**
     * @brief Wait until all bits in `events` become set.
     *
     * @param pEvent Pointer to event handle.
     * @param events Bit mask to wait for.
     * @param clearOnExit If `true`, clear the matched bits before returning successfully.
     * @param pMatchedEvents Output pointer receiving the matching bits.
     * @param waitTicks Maximum ticks to wait (`TASK_MAX_WAIT` for infinite wait).
     * @return `RET_SUCCESS` if all requested bits matched, `RET_TIMEOUT` on timeout,
     *         `RET_BUSY` if `waitTicks` is `TASK_NO_WAIT` and the full mask did not match,
     *         error code otherwise.
     */
    int eventWaitAll(eventHandleType *pEvent, uint32_t events,
                     bool clearOnExit, uint32_t *pMatchedEvents, uint32_t waitTicks);

    /**
     * @brief Atomically set bits, then wait for all bits in `waitEvents`.
     *
     * On success the matched bits are cleared before the call returns.
     *
     * @param pEvent Pointer to event handle.
     * @param setEvents Bit mask to set before waiting.
     * @param waitEvents Bit mask to wait for.
     * @param pMatchedEvents Output pointer receiving the matching bits.
     * @param waitTicks Maximum ticks to wait (`TASK_MAX_WAIT` for infinite wait).
     * @return `RET_SUCCESS` on success, `RET_TIMEOUT` on timeout,
     *         `RET_BUSY` if `waitTicks` is `TASK_NO_WAIT` and the full mask did not match,
     *         error code otherwise.
     */
    int eventSync(eventHandleType *pEvent, uint32_t setEvents,
                  uint32_t waitEvents, uint32_t *pMatchedEvents, uint32_t waitTicks);

#ifdef __cplusplus
}
#endif

#endif
