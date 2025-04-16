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
         * @param pLok  Pointer to the lock variable
         * @return Interrupt status flag
         * @retval true if interrupts were previously enabled
         * @retval false if interrupts are  previously disabled
         */
        static inline bool spinLock(atomic_t *pLock)
        {
                assert(pLock != NULL);
#if (CONFIG_SMP)
#if CONFIG_TASK_USER_MODE

                bool privileged = PORT_IS_PRIVILEGED(); // Check if CPU core was previously in privileged mode

                if (!privileged)
                {
                        PORT_ENTER_PRIVILEGED_MODE();
                }
#endif
                bool irqFlag = portIrqLock();

                while (!portAtomicCAS(pLock, 0, 1))
                {
                        PORT_NOP();
                }
                PORT_MEM_FENCE();
#if CONFIG_TASK_USER_MODE

                if (!privileged) // Exit privileged mode if CPU core was previously in unprivileged mode
                {
                        PORT_EXIT_PRIVILEGED_MODE();
                }
#endif

#else
        bool irqFlag = portIrqLock();

#endif

                return irqFlag;
        }

        /**
         * @brief Unlock the specified spinlock.
         *
         * @param pLock  Pointer to the lock variable.
         */
        static inline void spinUnlock(atomic_t *pLock, bool irqFlag)
        {
                assert(pLock != NULL);

#if (CONFIG_SMP)
                *pLock = 0;
#endif
                portIrqUnlock(irqFlag);
                PORT_MEM_FENCE();
        }

#ifdef __cplusplus
}
#endif

#endif