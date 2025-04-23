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
#include "sanoRTOS/port.h"

#ifdef __cplusplus
extern "C"
{
#endif

        typedef uint32_t atomic_t;

        /**
         * @brief Lock the specified spinlock.
         *
         * This function attempts to acquire a spinlock by using atomic operations. It ensures that the lock is
         * successfully obtained before proceeding. The interrupt status is also saved to restore later.
         *
         * @param pLock Pointer to the lock variable. This should point to an atomic lock variable.
         * @return Interrupt status flag.
         *         - `true` if interrupts were previously enabled before locking.
         *         - `false` if interrupts were previously disabled before locking.
         */
        static inline __attribute__((always_inline)) bool spinLock(atomic_t *pLock)
        {
                assert(pLock != NULL); ///< Ensure the lock pointer is not NULL.

#if (CONFIG_SMP)
#if CONFIG_TASK_USER_MODE

                bool privileged = PORT_IS_PRIVILEGED(); ///< Check if the CPU core is in privileged mode.

                if (!privileged)
                {
                        PORT_ENTER_PRIVILEGED_MODE(); ///< If in unprivileged mode, enter privileged mode to perform the lock operation.
                }
#endif

                bool irqState = portIrqLock(); ///< Save the interrupt status (whether interrupts are enabled or disabled).

                // Try to atomically acquire the lock using CAS (Compare-And-Swap).
                // It will keep retrying until the lock is successfully acquired.
                while (!portAtomicCAS(pLock, 0, 1))
                {
                        PORT_NOP(); ///< No-operation instruction to prevent tight spinning without performing any useful work.
                }

                PORT_MEM_FENCE(); ///< Ensure memory operations (like lock acquisition) are visible to other processors.

#if CONFIG_TASK_USER_MODE

                if (!privileged) ///< If the core was not in privileged mode, exit privileged mode.
                {
                        PORT_EXIT_PRIVILEGED_MODE();
                }

#endif

#else
        bool irqState = portIrqLock(); ///< Save the interrupt status for non-SMP systems.

#endif

                return irqState; ///< Return the interrupt status to restore it later (in the unlock function).
        }

        /**
         * @brief Unlock the specified spinlock.
         *
         * This function releases the spinlock, ensuring that the lock is cleared and any changes made
         * by the unlock operation are visible to other processors (if SMP is enabled).
         *
         * @param pLock  Pointer to the lock variable. This should point to an atomic lock variable.
         * @param irqState The flag that indicates whether interrupts were enabled prior to unlocking, used to restore the interrupt state.
         */
        static inline __attribute__((always_inline)) void spinUnlock(atomic_t *pLock, bool irqState)
        {
                assert(pLock != NULL); ///< Ensure the lock pointer is not NULL.

#if (CONFIG_SMP)
                *pLock = 0; ///< In SMP systems, clear the lock to release it. This ensures that the lock is released atomically.
#endif

                portIrqUnlock(irqState); ///< Restore the interrupt state (whether interrupts were enabled before unlocking).

                PORT_MEM_FENCE(); ///< Ensure proper memory ordering. This ensures that any changes to the lock are visible to other processors or tasks.
        }

#ifdef __cplusplus
}
#endif

#endif