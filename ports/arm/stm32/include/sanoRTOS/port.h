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
#include "stm32f4xx_hal.h"
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
        SWITCH_CONTEXT = 1,
        DISABLE_INTERRUPTS,
        ENABLE_INTERRUPTS,
        ENTER_PRIVILEGED_MODE

    } sysCodesType;

#define SYSTICK_PRIORITY 0xf // Priority of SysTick Timer.

#define PENDSV_PRIORITY 0xf // Priority of PendSV

#define BASEPRI(priority) ((priority) << (8 - __NVIC_PRIO_BITS))

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

#define PORT_TASK_STACK_DEFINE(name, stackSize, taskEntryFunction, taskExitFunction, taskParams) \
    uint32_t name##Stack[stackSize / sizeof(uint32_t)] = {                                       \
        [stackSize / sizeof(uint32_t) - 1] = 0x01000000,                                         \
        [stackSize / sizeof(uint32_t) - 2] = (uint32_t)taskEntryFunction,                        \
        [stackSize / sizeof(uint32_t) - 3] = (uint32_t)taskExitFunction,                         \
        [stackSize / sizeof(uint32_t) - 8] = (uint32_t)taskParams,                               \
        [stackSize / sizeof(uint32_t) - 9] = EXC_RETURN_THREAD_PSP};

/*Macro to invoke System call. This triggers SVC exception with specified sysCode*/
#define PORT_SYSCALL(sysCode) __asm volatile("svc %0" : : "I"(sysCode) : "memory");

#define PORT_ENTER_PRIVILEGED_MODE() PORT_SYSCALL(ENTER_PRIVILEGED_MODE)

#define PORT_EXIT_PRIVILEGED_MODE()           \
    do                                        \
    {                                         \
        __set_CONTROL(__get_CONTROL() | 0x1); \
        __ISB();                              \
    } while (0)

#if CONFIG_TASK_USER_MODE

#define PORT_DISABLE_INTERRUPTS() __set_BASEPRI(BASEPRI(1));

#define PORT_ENABLE_INTERRUPTS() __set_BASEPRI(0);

#else

#define PORT_DISABLE_INTERRUPTS() __disable_irq();

#define PORT_ENABLE_INTERRUPTS() __enable_irq();

#endif

#define PORT_NOP() __NOP()

#define PORT_MEM_FENCE() __DMB()

#define PORT_CORE_COUNT 1

#define PORT_CORE_ID() 0

#define PORT_TRIGGER_CONTEXT_SWITCH() TRIGGER_PENDSV()

#define PORT_TIMER_TICK_FREQ SystemCoreClock

#define PORT_PRINT printf

#define PORT_ENTER_SLEEP_MODE() __WFI()
    /**
     * @brief Check if CPU is executing in Privileged or Unprivileged mode
     *
     * @retval True, if cpu is in privileged mode
     * @retval False, if cpu is in unprivileged mode
     */
#define PORT_IS_PRIVILEGED() ((__get_IPSR() > 0) ? true : (((__get_CONTROL() & 0x1) == 0) ? true : false))

    /**
     * @brief Disable interrupts and return previous irq status
     *
     * @retval true, if interrupts were enabled previously
     * @retval false, if interrupts were disabled previously
     */
    static inline bool portIrqLock()
    {
        bool irqFlag;
#if CONFIG_TASK_USER_MODE

        irqFlag = (__get_BASEPRI() == 0);

        if (irqFlag)
        {

            if (PORT_IS_PRIVILEGED())
            {
                PORT_DISABLE_INTERRUPTS();
            }
            else
            {
                PORT_SYSCALL(DISABLE_INTERRUPTS);
            }
        }

#else
    irqFlag = (__get_PRIMASK() == 0);
    if (irqFlag)
    {

        PORT_DISABLE_INTERRUPTS();
    }

#endif

        return irqFlag;
    }

    /**
     * @brief Change interrupt status base on irqFlag
     *
     * @param irqFlag Flag representing previous irq status
     */
    static inline void portIrqUnlock(bool irqFlag)
    {
#if CONFIG_TASK_USER_MODE

        if (irqFlag)
        {
            if (PORT_IS_PRIVILEGED())
            {
                PORT_ENABLE_INTERRUPTS();
            }
            else
            {
                PORT_SYSCALL(ENABLE_INTERRUPTS);
            }
        }

#else
    if (irqFlag)
    {
        PORT_ENABLE_INTERRUPTS();
    }
#endif
    }

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
        uint32_t old_val;

        __asm__ volatile(
            "1:                     \n"
            "ldrex %0, [%2]         \n" // Load *ptr into old_val
            "cmp %0, %3             \n" // Compare with expected
            "bne 2f                 \n" // If not equal, branch to fail
            "strex %1, %4, [%2]     \n" // Try to store set_val
            "cmp   %1, #0           \n" // check if STREX succeeded
            "bne   1b               \n" // if not, retry loop
            "2:                     \n"
            : "=&r"(old_val), "=&r"(status)
            : "r"(ptr), "r"(compare_val), "r"(set_val)
            : "cc", "memory");

        return (old_val == compare_val);
    }

#ifdef __cplusplus
}
#endif

#endif
