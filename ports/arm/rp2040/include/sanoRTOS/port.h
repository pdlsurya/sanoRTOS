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

#ifndef __SANORTOS_PORT_H
#define __SANORTOS_PORT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sanoRTOS/config.h"
#include "cmsis_gcc.h"
#include "RP2040.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*Forward declaration of taskHandleType*/
    typedef struct taskHandle taskHandleType;

#define PENDSV_PRIORITY 0xf /* Priority of PendSV */
#define SYSTICK_PRIORITY 0xf /* Priority of SysTick Timer */
#define PORT_ATOMIC_SPINLOCK_ID PICO_SPINLOCK_ID_OS1
#define TRIGGER_PENDSV() (SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk)

    /**********--Task's default stack contents--****************************************
          ____ <-- stackBase = stack + stackSize / sizeof(uint32_t)
         |____|xPSR  --> stackBase - 1
         |____|Return address(PC) <-Task Entry --> stackBase - 2
         |____|LR --> stackBase - 3
         |____|R12
         |____|R3
         |____|R2
         |____|R1
         |____|R0 <-Task params --> stackBase - 8
         |____|EXC_RETURN --> stackBase - 9

        <--Cortex-M3/M4/M7/M33-->               <--Cortex-M0/M0+--->
         |____|R11                                 |____|R7
         |____|R10                                 |____|R6
         |____|R9                                  |____|R5
         |____|R8                                  |____|R4
         |____|R7                                  |____|R11
         |____|R6                                  |____|R10
         |____|R5                                  |____|R9
         |____|R4 <--stackPointer=(stackBase - 17) |____|R8 <--stackPointer=(stackBase - 17)
            |                                         |
            |                                         |
         |____|                                    |____|
         |____|                                    |____|
       <-32bits->                                 <-32bits->
      *************************************************************************************/

#define PORT_TASK_STACK_DEFINE(name, stackSize, taskEntryFunction, taskExitFunction, taskParams) \
    static uint32_t name##Stack[stackSize / sizeof(uint32_t)] = {                                \
        [stackSize / sizeof(uint32_t) - 1] = 0x01000000,                                         \
        [stackSize / sizeof(uint32_t) - 2] = (uint32_t)taskEntryFunction,                        \
        [stackSize / sizeof(uint32_t) - 3] = (uint32_t)taskExitFunction,                         \
        [stackSize / sizeof(uint32_t) - 8] = (uint32_t)taskParams,                               \
        [stackSize / sizeof(uint32_t) - 9] = EXC_RETURN_THREAD_PSP}

#define PORT_TASK_STACK_INIT(stack, stackWords, taskEntryFunction, taskExitFunction, taskParams) \
    do                                                                                             \
    {                                                                                              \
        (stack)[(stackWords)-1] = 0x01000000;                                                     \
        (stack)[(stackWords)-2] = (uint32_t)(taskEntryFunction);                                  \
        (stack)[(stackWords)-3] = (uint32_t)(taskExitFunction);                                   \
        (stack)[(stackWords)-8] = (uint32_t)(taskParams);                                         \
        (stack)[(stackWords)-9] = EXC_RETURN_THREAD_PSP;                                          \
    } while (0)

#define PORT_INITIAL_TASK_STACK_OFFSET 17

#define PORT_TRIGGER_CONTEXT_SWITCH() TRIGGER_PENDSV()

#define PORT_NOP() __NOP()

#define PORT_MEM_FENCE() __DMB()

#if CONFIG_SMP
#define PORT_CORE_COUNT 2
#else
#define PORT_CORE_COUNT 1
#endif

#define PORT_TIMER_TICK_FREQ SystemCoreClock

#define PORT_ENTER_SLEEP_MODE() __WFI()

#define PORT_CORE_ID() get_core_num()

#define PORT_PRINTF printf

#define PORT_DISABLE_INTERRUPTS() __disable_irq()

#define PORT_ENABLE_INTERRUPTS() __enable_irq()

    /**
     * @brief Disable interrupts and return previous irq status
     *
     * @retval `true`, if interrupts were enabled previously
     * @retval `false`, if interrupts were disabled previously
     */
    static inline bool portIrqEnabled()
    {
        return (__get_PRIMASK() == 0U);
    }

    static inline bool portIrqLock()
    {
        bool irqState = portIrqEnabled();
        if (irqState)
        {
            PORT_DISABLE_INTERRUPTS();
        }

        return irqState;
    }

    static inline uint32_t portGetCurrentStackPointer()
    {
        return __get_PSP();
    }

    /**
     * @brief Change interrupt status based on irqState
     *
     * @param irqState Flag representing previous irq status
     */
    static inline void portIrqUnlock(bool irqState)
    {
        if (irqState)
        {
            PORT_ENABLE_INTERRUPTS();
        }
    }

    /**
     * @brief Compare-And-Set function for RP2040.
     *
     * Cortex-M0+ does not provide the exclusive-access instructions used by the
     * other ARM ports, so this path serializes CAS with a dedicated RP2040
     * hardware spin lock reserved for the RTOS.
     *
     * @param ptr Pointer to the target memory location
     * @param compare_val The expected old value
     * @param set_val The new value to be stored if `*ptr` is equal to `compare_val`
     * @return `true` if the set was successful, `false` otherwise
     */
    static inline bool portAtomicCAS(volatile uint32_t *ptr, uint32_t compare_val, uint32_t set_val)
    {
        spin_lock_t *lock = spin_lock_instance(PORT_ATOMIC_SPINLOCK_ID);
        uint32_t saved_irq = spin_lock_blocking(lock);
        bool success = (*ptr == compare_val);
        if (success)
        {
            *ptr = set_val;
        }
        spin_unlock(lock, saved_irq);
        PORT_MEM_FENCE();
        return success;
    }

    /**
     * @brief Configure platform specific core components and
     * start the RTOS scheduler by jumping to the first task.
     */
    void portSchedulerStart();

#ifdef __cplusplus
}
#endif

#endif
