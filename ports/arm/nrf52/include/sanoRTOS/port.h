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
#include <string.h>
#include "nrf_soc.h"
#include "cmsis_gcc.h"
#include "sanoRTOS/config.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Codes used by System call to perform a specified action.
     *
     */
    typedef enum
    {
        DISABLE_INTERRUPTS = 1,
        ENABLE_INTERUPPTS,
        SWITCH_CONTEXT,
    } sysCodesType;

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

        <--Cortex-M3/M4/M7-->               <--Cortex-MO/M0+--->
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

#define INITIAL_TASK_STACK_OFFSET 17

#define TASK_STACK_DEFINE(name, stackSize, taskEntryFunction, taskExitFunction, taskParams) \
    uint32_t name##Stack[stackSize / sizeof(uint32_t)] = {                                  \
        [stackSize / sizeof(uint32_t) - 1] = 0x01000000,                                    \
        [stackSize / sizeof(uint32_t) - 2] = (uint32_t)taskEntryFunction,                   \
        [stackSize / sizeof(uint32_t) - 3] = (uint32_t)taskExitFunction,                    \
        [stackSize / sizeof(uint32_t) - 8] = (uint32_t)taskParams,                          \
        [stackSize / sizeof(uint32_t) - 9] = EXC_RETURN_THREAD_PSP};

#define PENDSV_PRIORITY 0xff // Priority of PendSV

#define SYSTICK_PRIORITY 0xff // SysTick Priority

/*Macro to invoke System call. This triggers SVC exception with specified sysCode*/
#define SYSCALL(sysCode) __asm volatile("svc %0" : : "I"(sysCode) : "memory");

#define SYSTICK_INT_DISABLE() (SysTick->CTRL &= ~(SysTick_CTRL_TICKINT_Msk))
#define SYSTICK_INT_ENABLE() (SysTick->CTRL |= (SysTick_CTRL_TICKINT_Msk))

#define PORT_DISABLE_IRQ()     \
    do                         \
    {                          \
        __set_BASEPRI(1);      \
        SYSTICK_INT_DISABLE(); \
    } while (0)

#define PORT_ENABLE_IRQ()     \
    do                        \
    {                         \
        __set_BASEPRI(0);     \
        SYSTICK_INT_ENABLE(); \
    } while (0)

#if (CONFIG_TASK_USER_MODE)
#define ENTER_CRITICAL_SECTION() SYSCALL(DISABLE_INTERRUPTS)
#define EXIT_CRITICAL_SECTION() SYSCALL(ENABLE_INTERUPPTS)

#else
#define ENTER_CRITICAL_SECTION() PORT_DISABLE_IRQ() /*Disable all the interrupts with priority values 1 or higher*/
#define EXIT_CRITICAL_SECTION() PORT_ENABLE_IRQ()
#endif

#if (CONFIG_TASK_USER_MODE)
#define PORT_IRQ_LOCK() SYSCALL(DISABLE_INTERRUPTS)
#define PORT_IRQ_UNLOCK() SYSCALL(ENABLE_INTERUPPTS)

#else
#define PORT_IRQ_LOCK() PORT_DISABLE_IRQ() /*Disable all the interrupts with priority values 1 or higher*/
#define PORT_IRQ_UNLOCK() PORT_ENABLE_IRQ()
#endif

#define PORT_NOP() __NOP()

#define CORE_ID() (0)

#define portTriggerContextSwitch() (SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk)

#define CPU_FREQ SystemCoreClock

#define SYSTICK_HANDLER SysTick_Handler

#define SYSTICK_CONFIG() SysTick_Config(OS_INTERVAL_CPU_TICKS)

    /**
     * @brief Configure platform specific core components and
     * start the RTOS scheduler by jumping to the first task.
     *
     * @param pTask  Pointer to the first task to be executed
     */
    void portSchedulerStart();

    /**
     * @brief Compare-And-Set function for ARM Cortex-M
     *
     * @param ptr Pointer to the target memory location
     * @param compare_val The expected old value
     * @param set_val The new value to be stored if `*ptr` is equal to `expected`
     * @return bool Returns true if the set was successful, false otherwise
     */
    static inline bool portAtomicCAS(volatile uint32_t *ptr, uint32_t compare_val, uint32_t set_val)
    {
        uint32_t status;

        do
        {
            uint32_t old_val = __LDREXW(ptr); // Load-exclusive
            if (old_val != compare_val)
            {
                // __CLREX();    // Clear exclusive if comparison fails
                return false; // Indicate failure
            }
            status = __STREXW(set_val, ptr); // Store-exclusive (returns 0 on success)
        } while (status != 0); // Retry if store fails

        return true; // Success
    }

#ifdef __cplusplus
}
#endif

#endif
