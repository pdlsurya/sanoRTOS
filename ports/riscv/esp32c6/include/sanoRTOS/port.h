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

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "sanoRTOS/config.h"
#include "riscv/rv_utils.h"
#include "interrupts.h"
#include "mtimer.h"
#include "usb_serial.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*Forward declaration of taskHandleType*/
    typedef struct taskHandle taskHandleType;

    /**
     * @brief Codes used by System call to perform a specified action.
     *
     */

    typedef enum
    {
        SWITCH_CONTEXT = 1,
        DISABLE_INTERRUPTS,
        ENABLE_INTERRUPTS,
        ENTER_PRIVILEGED_MODE,
        EXIT_PRIVILEGED_MODE,
        GET_PRIVILEGE_MODE

    } sysCodesType;

    typedef enum
    {
        MACHINE_MODE,
        USER_MODE
    } privilegeModesType;

    typedef void (*tickHandlerType)(void);

    /**********--Task's default stack contents--****************************************

              ____<-- stackBase = stack + stackSize / sizeof(uint32_t)
             |____|
             |____|
             |____| t6 (stackBase - 3)
             |____| t5
             |____| t4
             |____| t3
             |____| s11
             |____| s10
             |____| s9
             |____| s8
             |____| s7
             |____| s6
             |____| s5
             |____| s4
             |____| s3
             |____| s2
             |____| a7
             |____| a6
             |____| a5
             |____| a4
             |____| a3
             |____| a2
             |____| a1
             |____| a0 (stackBase-24)<- Task params
             |____| s1
             |____| s0
             |____| t2
             |____| t1
             |____| t0
             |____| tp
             |____| ra   (stackBase-31)<--- return address
             |____| mepc (stackBase-32)
                |
                |
             |____|
             |____|
           <-32bits->
          *************************************************************************************/
#define PORT_TASK_STACK_DEFINE(name, stackSize, taskEntryFunction, taskExitFunction, taskParams) \
  static  uint32_t name##Stack[stackSize / sizeof(uint32_t)] = {                                       \
        [stackSize / sizeof(uint32_t) - 32] = (uint32_t)taskEntryFunction,                       \
        [stackSize / sizeof(uint32_t) - 31] = (uint32_t)taskExitFunction,                        \
        [stackSize / sizeof(uint32_t) - 24] = (uint32_t)taskParams}

#define INITIAL_TASK_STACK_OFFSET 32

/*Macro to invoke System call. This triggers SVC exception with specified sysCode*/
#define PORT_SYSCALL(sysCode) asm volatile("mv a0,%0\n" \
                                           "ecall" : : "r"(sysCode));

#define PORT_DISABLE_INTERRUPTS() rv_utils_intr_global_disable();

#define PORT_ENABLE_INTERRUPTS() rv_utils_intr_global_enable();

#define PORT_ENTER_PRIVILEGED_MODE() PORT_SYSCALL(ENTER_PRIVILEGED_MODE)

#define PORT_EXIT_PRIVILEGED_MODE() PORT_SYSCALL(EXIT_PRIVILEGED_MODE)

#define PORT_IS_PRIVILEGED() isMachineMode()

#define PORT_TIMER_TICK_FREQ 160000000

#define PORT_CORE_COUNT 1

#define PORT_CORE_ID()  portCoreId()

#define PORT_MEM_FENCE() asm volatile("fence rw, rw\n")

#define PORT_TRIGGER_CONTEXT_SWITCH() msi_trigger()

#define PORT_NOP() asm volatile("nop")

#define PORT_ENTER_SLEEP_MODE() asm volatile("wfi");

#define PORT_PRINT serial_printf

    volatile extern privilegeModesType privilegeMode;

    /**
     * @brief Check if cpu is executing in machine mode
     *
     * @retval `true`, if cpu is executing in machine mode
     * @retval `false`, otherwise
     */
    static inline bool isMachineMode()
    {
        PORT_SYSCALL(GET_PRIVILEGE_MODE);

        return (privilegeMode == MACHINE_MODE);
    }

    /**
     * @brief Get current hart id
     *
     * @return Current hart id
     */

    static inline uint8_t portCoreId()
    {

#if CONFIG_TASK_USER_MODE
        if (!PORT_IS_PRIVILEGED())
        {
            PORT_ENTER_PRIVILEGED_MODE();

            uint32_t coreId = RV_READ_CSR(mhartid);

            PORT_EXIT_PRIVILEGED_MODE();

            return coreId;
        }
        else
        {
            return RV_READ_CSR(mhartid);
        }

#else
    return RV_READ_CSR(mhartid);

#endif
    }

    /**
     * @brief Check if interrupts are enabled
     *
     * @retval `true`, if interrupts are enabled
     * @retval `false`, if interrupts are disabled
     */
    static inline bool portIrqEnabled()
    {
#if CONFIG_TASK_USER_MODE
        if (!PORT_IS_PRIVILEGED())
        {
            PORT_ENTER_PRIVILEGED_MODE();
            bool irqState = (RV_READ_CSR(mstatus) & MSTATUS_MIE);
            PORT_EXIT_PRIVILEGED_MODE();
            return irqState;
        }
        else
        {
            return (RV_READ_CSR(mstatus) & MSTATUS_MIE);
        }

#else

    return (RV_READ_CSR(mstatus) & MSTATUS_MIE);
#endif
    }

    /**
     * @brief Disable interrupts and return previous irq status
     *
     * @retval `true`, if interrupts were enabled previously
     * @retval `false`, if interrupts were disabled previously
     */
    static inline bool portIrqLock()
    {
        bool irqState = portIrqEnabled();

#if CONFIG_TASK_USER_MODE
        if (irqState)
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
    if (irqState)
    {
        PORT_DISABLE_INTERRUPTS();
    }
#endif
        return irqState;
    }

    /**
     * @brief Change interrupt status base on irqState
     *
     * @param irqState Flag representing previous irq status
     */
    static inline void portIrqUnlock(bool irqState)
    {
#if CONFIG_TASK_USER_MODE

        if (irqState)
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
    if (irqState)
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
     * @return `true` if the set was successful, false otherwise
     */
    static inline bool portAtomicCAS(volatile uint32_t *ptr, uint32_t compare_val, uint32_t set_val)
    {
        return rv_utils_compare_and_set(ptr, compare_val, set_val);
    }

#ifdef __cplusplus
}
#endif

#endif
