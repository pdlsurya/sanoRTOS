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

    typedef void (*tickHandlerType)(void);

    /**********--Task's default stack contents--****************************************

              ____<-- stackBase = stack + stackSize / sizeof(uint32_t)
             |____| t6 (stackBase - 1)
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
             |____| a0 (stackBase-22)<- Task params
             |____| s1
             |____| s0
             |____| t2
             |____| t1
             |____| t0
             |____| tp
             |____| gp
             |____| sp (stackBase-30) not used
             |____| ra   (stackBase-31)<--- return address
             |____| mepc (stackBase-32)
                |
                |
             |____|
             |____|
           <-32bits->
          *************************************************************************************/
#define PORT_TASK_STACK_DEFINE(name, stackSize, taskEntryFunction, taskExitFunction, taskParams) \
    static uint32_t name##Stack[stackSize / sizeof(uint32_t)] = {                                \
        [stackSize / sizeof(uint32_t) - 32] = (uint32_t)taskEntryFunction,                       \
        [stackSize / sizeof(uint32_t) - 31] = (uint32_t)taskExitFunction,                        \
        [stackSize / sizeof(uint32_t) - 22] = (uint32_t)taskParams}

#define PORT_TASK_STACK_INIT(stack, stackWords, taskEntryFunction, taskExitFunction, taskParams) \
    do                                                                                           \
    {                                                                                            \
        (stack)[(stackWords) - 32] = (uint32_t)(taskEntryFunction);                              \
        (stack)[(stackWords) - 31] = (uint32_t)(taskExitFunction);                               \
        (stack)[(stackWords) - 22] = (uint32_t)(taskParams);                                     \
    } while (0)

#define PORT_INITIAL_TASK_STACK_OFFSET 32

#define PORT_DISABLE_INTERRUPTS() rv_utils_intr_global_disable();

#define PORT_ENABLE_INTERRUPTS() rv_utils_intr_global_enable();

#define PORT_TIMER_TICK_FREQ 360000000

#if CONFIG_SMP
#define PORT_CORE_COUNT 2
#else
#define PORT_CORE_COUNT 1
#endif

#define PORT_CORE_ID() portCoreId()

#define PORT_MEM_FENCE() asm volatile("fence rw, rw\n")

#define PORT_TRIGGER_CONTEXT_SWITCH() msi_trigger()

#define PORT_NOP() asm volatile("nop")

#define PORT_ENTER_SLEEP_MODE() asm volatile("wfi");

#define PORT_PRINTF serial_printf

    /**
     * @brief Get current hart id
     *
     * @return Current hart id
     */

    static inline uint8_t portCoreId()
    {
        return RV_READ_CSR(mhartid);
    }

    /**
     * @brief Check if interrupts are enabled
     *
     * @retval `true`, if interrupts are enabled
     * @retval `false`, if interrupts are disabled
     */
    static inline bool portIrqEnabled()
    {
        return (RV_READ_CSR(mstatus) & MSTATUS_MIE);
    }

    /**
     * @brief Check whether the current execution context is interrupt/exception handler mode.
     *
     * @retval `true` Current context is ISR/handler context.
     * @retval `false` Current context is normal task/thread context.
     */
    static inline bool portIsInISRContext()
    {
        uint32_t mstatus = RV_READ_CSR(mstatus);
        uint32_t mcause = RV_READ_CSR(mcause);
        return (((mstatus & MSTATUS_MIE) == 0U) && (((mcause >> 31U) & 0x1U) != 0U));
    }

    static inline uint32_t portGetCurrentStackPointer()
    {
        uint32_t stackPointer;

#if defined(USE_ISR_STACK) && USE_ISR_STACK
        if (portIsInISRContext())
        {
            asm volatile("csrr %0, mscratch" : "=r"(stackPointer));
            return stackPointer;
        }
#endif

        asm volatile("mv %0, sp" : "=r"(stackPointer));
        return stackPointer;
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
        if (irqState)
        {
            PORT_DISABLE_INTERRUPTS();
        }
        return irqState;
    }

    /**
     * @brief Change interrupt status base on irqState
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
