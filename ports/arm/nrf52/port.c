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

#include <stdlib.h>
#include <sanoRTOS/port.h>
#include "sanoRTOS/config.h"
#include "sanoRTOS/task.h"

/*RTOS tick handler function*/
extern void tickHandler(void);

/**
 * @brief Configure platform specific interrupts and tick timer.
 */
static inline void portConfig()
{

    NVIC_SetPriority(SysTick_IRQn, SYSTICK_PRIORITY);

    /* Assign lowest priority to PendSV*/
    NVIC_SetPriority(PendSV_IRQn, PENDSV_PRIORITY);

    /* Configure SysTick to generate interrupt every TIMER_TICKS_PER_RTOS_TICK */
    SysTick_Config(TIMER_TICKS_PER_RTOS_TICK);
}

/**
 * @brief Setup the scheduler to start the first task.
 *
 * @param pTask
 */
void portSchedulerStart()
{

    /*Get the highest priority ready task from ready Queue*/
    currentTask[PORT_CORE_ID()] = taskPool.currentTask[PORT_CORE_ID()] = taskQueueGet(&taskPool.readyQueue);

    /*Change status to RUNNING*/
    currentTask[PORT_CORE_ID()]->status = TASK_STATUS_RUNNING;

    portConfig();

    __set_PSP(currentTask[PORT_CORE_ID()]->stackPointer); /* Set PSP to the top of task's stack */

    uint32_t control = __get_CONTROL(); // Read CONTROL register

    __set_CONTROL(CONFIG_TASK_USER_MODE ? (control | 0x3) : control | 0x2); // Set bit 0 to 1 to switch to unprivileged mode

    __ISB(); // Instruction Synchronization Barrier

    __DSB(); // Data Synchronization Barrier

    /*Jump to first task*/
    currentTask[PORT_CORE_ID()]->taskEntry(currentTask[PORT_CORE_ID()]->params);
}

/**
 * @brief SysTick Timer interrupt handler. It selects next task to run and
 * triggers PendSV to perform actual context switch.
 */
void SysTick_Handler()
{
    tickHandler();
}

/**
 * @brief SVC interrupt service routine(ISR). SVC interrupt is triggered via SYSCALL
 * with a specific SVC number. SVC number is decoded to perform corresponding action.
 *
 */
void SVC_Handler()
{

    uint32_t exc_return;
    __asm__ volatile("mov %0, lr" : "=r"(exc_return)::"lr", "memory");

    uint32_t *stackPointer;

    if (exc_return & 0x4)
    {
        stackPointer = (uint32_t *)__get_PSP();
    }
    else
    {
        stackPointer = (uint32_t *)__get_MSP();
    }

    /*Get PC from exception stack frame which hold address of instruction after the SVC instruction
    ____ <-- stackBase
   |____|xPSR stackPointer[7]
   |____|PC stackPointer[6]
   |____|LR stackPointer[5]
   |____|R12 stackPointer[4]
   |____|R3 stackPointer[3]
   |____|R2 stackPointer[2]
   |____|R1 stackPointer[1]
   |____|R0 stackPointer[0]*/

    uint32_t PC = stackPointer[6];

    /*Extract the SVC number from the SVC instruction, which resides just before the address in the PC.
     The SVC instruction is a 16-bit instruction, whose LSB byte is the SVC number.
     Hence, the SVC number is extracted by accessing the value at the memory location which is two bytes before the address in the PC.*/
    uint8_t svcNumber = *((uint8_t *)PC - 2);

    switch (svcNumber)
    {
    case SWITCH_CONTEXT:
        PORT_TRIGGER_CONTEXT_SWITCH();
        break;

    case DISABLE_INTERRUPTS:
        PORT_DISABLE_INTERRUPTS();
        break;

    case ENABLE_INTERRUPTS:
        PORT_ENABLE_INTERRUPTS();
        break;
    case ENTER_PRIVILEGED_MODE:
        __set_CONTROL(__get_CONTROL() & ~0x1);
        __ISB();
        break;
    }
}

/**
 * @brief PendSV Handler for context switching between tasks.
 *
 * This function is triggered by the PendSV exception and performs
 * a context switch by saving the state (registers and optionally FPU registers)
 * of the currently running task and restoring the state of the next task.
 *
 * It uses inline assembly to:
 * - Save r4–r11 and LR of the current task to its stack.
 * - Conditionally save FPU registers s16–s31 if the task used the FPU.
 * - Update the `currentTask` stack pointer.
 * - Load the stack pointer of the `nextTask`.
 * - Restore r4–r11, LR, and optionally FPU registers s16–s31.
 * - Update the process stack pointer (PSP) for the new task.
 *
 * Synchronization barriers (DMB/ISB) are used to ensure proper
 * memory and pipeline ordering, ensuring consistent task switching.
 *
 * @note This function is marked as `naked` and must not contain
 *       any C code outside inline assembly. Only called by the
 *       Cortex-M exception mechanism (PendSV).
 */

__attribute__((naked)) void PendSV_Handler(void)
{
    __asm volatile(
        "cpsid i\n" // Disable interrupts

        "mrs r0, psp\n" // Get current process stack pointer

        // If task uses FPU, save s16–s31
        "tst lr, #0x10\n"
        "it eq\n"
        "vstmdbeq r0!, {s16-s31}\n"

        // Save r4-r11 and lr to current task's stack
        "stmdb r0!, {r4-r11, lr}\n"

        "dmb\n" // Ensure all stores complete before task switch

        // Save current stack pointer to *currentTask
        "str r0, [%[current]]\n"

        // Load next stack pointer from *nextTask
        "ldr r0, [%[next]]\n"

        "dmb\n" // Ensure we loaded the correct value

        // Restore r4-r11 and lr
        "ldmia r0!, {r4-r11, lr}\n"

        // If task uses FPU, restore s16–s31
        "tst lr, #0x10\n"
        "it eq\n"
        "vldmiaeq r0!, {s16-s31}\n"

        "msr psp, r0\n" // Set PSP

        "isb\n" // Flush instruction pipeline before return

        "cpsie i\n" // Enable interrupts

        "bx lr\n" // Return to Thread mode
        :
        : [current] "r"(&currentTask[PORT_CORE_ID()]->stackPointer),
          [next] "r"(&nextTask[PORT_CORE_ID()]->stackPointer)
        : "r0", "memory");
}
