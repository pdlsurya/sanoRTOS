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
#include <stdint.h>
#include <string.h>
#include "sanoRTOS/config.h"
#include "riscv/rv_utils.h"
#include "interrupts.h"
#include "mtimer.h"

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
        DISABLE_INTERRUPTS = 1,
        ENABLE_INTERUPPTS,
        SWITCH_CONTEXT
    } sysCodesType;

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
#define TASK_STACK_DEFINE(name, stackSize, taskEntryFunction, taskExitFunction, taskParams) \
    uint32_t name##Stack[stackSize / sizeof(uint32_t)] = {                                  \
        [stackSize / sizeof(uint32_t) - 32] = (uint32_t)taskEntryFunction,                  \
        [stackSize / sizeof(uint32_t) - 31] = (uint32_t)taskExitFunction,                   \
        [stackSize / sizeof(uint32_t) - 24] = (uint32_t)taskParams};

#define INITIAL_TASK_STACK_OFFSET 32

/*Macro to invoke System call. This triggers SVC exception with specified sysCode*/
#define SYSCALL(sysCode) asm volatile("mv a0,%0\n" \
                                      "ecall" : : "r"(sysCode));

#define PORT_DISABLE_IRQ() rv_utils_intr_global_disable()
#define PORT_ENABLE_IRQ() rv_utils_intr_global_enable()

#if (CONFIG_TASK_USER_MODE)
#define ENTER_CRITICAL_SECTION() SYSCALL(DISABLE_INTERRUPTS)
#define EXIT_CRITICAL_SECTION() SYSCALL(ENABLE_INTERUPPTS)

#else
#define ENTER_CRITICAL_SECTION() PORT_DISABLE_IRQ()
#define EXIT_CRITICAL_SECTION() PORT_ENABLE_IRQ()
#endif

#define CPU_FREQ 160000000

    /**
     * @brief Trigger context switch
     *
     */

#define portTriggerContextSwitch() msi_trigger()

#define PORT_NOP() asm volatile("nop")

    /**
     * @brief Configure platform specific core components and
     * start the RTOS scheduler by jumping to the first task.
     *
     * @param pTask  Pointer to the first task to be executed
     */
    void portSchedulerStart(taskHandleType *pTask);

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
        return rv_utils_compare_and_set(ptr, compare_val, set_val);
    }

#ifdef __cplusplus
}
#endif

#endif
