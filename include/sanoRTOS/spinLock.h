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

#ifndef __SANO_RTOS_SPIN_LOCK_H
#define __SANO_RTOS_SPIN_LOCK_H

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "sanoRTOS/config.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/taskQueue.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef uint32_t atomic_t;

    /**
     * @brief Lock the specified spinlock.
     *
     * @param pLok  Pointer to the lock variable
     */
    static inline void spinLock(atomic_t *pLock)
    {
        assert(pLock != NULL);

        // Disable interrupts to avoid context switch
        PORT_IRQ_LOCK();

#if (CONFIG_SMP)
        while (!portAtomicCAS(pLock, 0, 1))
        {
            PORT_NOP();
        }
#endif
    }

    /**
     * @brief Unlock the specified spinlock.
     *
     * @param pLock  Pointer to the lock variable.
     */
    static inline void spinUnlock(atomic_t *pLock)
    {
        assert(pLock != NULL);

#if (CONFIG_SMP)
        *pLock = 0;
#endif

        // Enable interrupts
        PORT_IRQ_UNLOCK();
    }

#ifdef __cplusplus
}
#endif

#endif