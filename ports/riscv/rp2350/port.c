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
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "sanoRTOS/port.h"
#include "sanoRTOS/config.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/spinLock.h"
#include "sanoRTOS/log.h"

#if (CONFIG_SMP)

TASK_DEFINE(idleTask1, 512, idleTaskHandler1, NULL, TASK_LOWEST_PRIORITY, AFFINITY_CORE_1);

void idleTaskHandler1(void *params)
{
    (void)params;
    while (1)
    {
        PORT_ENTER_SLEEP_MODE();
    }
}
#endif

/*RTOS tick handler function*/
extern void tickHandler(void);

volatile privilegeModesType privilegeMode = MACHINE_MODE;

static uint64_t mtimer_period_ticks;

static atomic_t lock;

/**
 * @brief rtos system call handler
 *
 * @param sysCode
 */
void syscallHandler(uint32_t sysCode)
{
    switch (sysCode)
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
        riscv_set_csr(mstatus, RVCSR_MSTATUS_MPP_BITS);
        break;

    case EXIT_PRIVILEGED_MODE:
        riscv_clear_csr(mstatus, RVCSR_MSTATUS_MPP_BITS);
        break;

    case GET_PRIVILEGE_MODE:
        privilegeMode = ((riscv_read_csr(mstatus) & RVCSR_MSTATUS_MPP_BITS) != 0) ? MACHINE_MODE : USER_MODE;
        break;

    default:
        break;
    }
}

/**
 * @brief Configure rtos tick timer.
 *
 */
static inline void portTickConfig()
{
    mtimer_period_ticks = TIMER_TICKS_PER_RTOS_TICK;

    sio_hw->mtime_ctrl = SIO_MTIME_CTRL_EN_BITS;

    SET_MTIMECMP((GET_MTIME() + mtimer_period_ticks));

    riscv_set_csr(mie, RVCSR_MIE_MTIE_BITS);
    riscv_set_csr(mstatus, RVCSR_MSTATUS_MIE_BITS);
}

/**
 * @brief Configure platform specific interrupts and tick timer.
 *
 */
static inline void portConfig()
{
    /*Replace this with vendor specific machine software interrupt enable api and
    machine timer interrupt setup api */

    /*Enable risc-v machine software interrupt*/
    riscv_set_csr(mie, RVCSR_MIE_MSIE_BITS);
    riscv_set_csr(mstatus, RVCSR_MSTATUS_MIE_BITS);
    /*initialize mtimer to generate interrupt every 1 ms*/
    portTickConfig();
}

/**
 * @brief Run the first task.
 *
 */
void portRunFirstTask()
{
    bool irqFlag = spinLock(&lock);
    /*Get the highest priority ready task from ready Queue*/
    currentTask[PORT_CORE_ID()] = taskPool.currentTask[PORT_CORE_ID()] = taskQueueGet(&taskPool.readyQueue);

    /*Change status to RUNNING*/
    currentTask[PORT_CORE_ID()]->status = TASK_STATUS_RUNNING;

    portConfig();

#if USE_ISR_STACK
    /*Save main sp to mscratch so that exception/interrupt handler can retrieve it during an
     exception/interrupt and use its own stack*/
    __asm__ volatile("csrw mscratch, sp");

#endif

    __asm__ volatile(" mv sp, %0" ::"r"(currentTask[PORT_CORE_ID()]->stackPointer));

/*If user mode is enabled, Physical Memory Protection(PMP) and Access Permission Management(APM)
  should be configured to allow user mode access to memory and peripheral regions */
#if (CONFIG_TASK_USER_MODE)

    // pmpConfigure();

    /* Clear MPP bits of mstatus to switch to user mode(U-mode)*/
    riscv_clear_csr(mstatus, RVCSR_MSTATUS_MPP_BITS);

    /* MPIE is set to 1 to restore interrupt enable state after the transition.*/
    riscv_set_csr(mstatus, RVCSR_MSTATUS_MPIE_BITS);

    /* Set up the user-mode entry point*/
    riscv_write_csr(mepc, (uint32_t)currentTask[PORT_CORE_ID()]->taskEntry);

    spinUnlock(&lock, irqFlag);

    // Execute mret to switch to U-mode
    asm volatile("mret");

#else
    spinUnlock(&lock, irqFlag);
    /*If user mode not enabled, jump directly to the first task*/
    currentTask[PORT_CORE_ID()]->taskEntry(currentTask[PORT_CORE_ID()]->params);
#endif
}

/**
 * @brief Entry point for core 1
 *
 */
static void core1_entry(void)
{

    portRunFirstTask();
}

/**
 * @brief Setup the scheduler to start the first task.
 *
 */
void portSchedulerStart()
{
#if (CONFIG_SMP)
    taskStart(&idleTask1);

    multicore_launch_core1(core1_entry);
#endif

    portRunFirstTask();
}

__attribute__((interrupt)) __attribute__((section(".time_critical"))) void isr_riscv_machine_timer()
{
#if USE_ISR_STACK
    /*Load sp with isr/handler mode stack pointer stored in mscratch and store current thread mode sp to mscratch*/
    __asm__ volatile("csrrw sp,mscratch,sp");
#endif
    SET_MTIMECMP((GET_MTIME() + mtimer_period_ticks));

    /*Store mcause, mepc and mstatus in local variables before enabling interrupt nesting*/
    uint32_t mcause_val = riscv_read_csr(mcause);
    uint32_t mepc_val = riscv_read_csr(mepc);
    uint32_t mstatus_val = riscv_read_csr(mstatus);

    // Invoke RTOS tickHandler function
    tickHandler();
   // LOG_DEBUG("timer", "Timer on core=%d\n", PORT_CORE_ID());

    /* Restore mstatus, mepc and mcause*/
    riscv_write_csr(mstatus, mstatus_val);
    riscv_write_csr(mcause, mcause_val);
    riscv_write_csr(mepc, mepc_val);

#if USE_ISR_STACK
    /*Load sp with thread  mode stack pointer stored in mscratch and store current isr/handler mode sp to mscratch*/
    __asm__ volatile("csrrw sp,mscratch,sp");
#endif
}

__attribute__((section(".time_critical"))) void isr_riscv_machine_ecall_mmode_exception()
{
    uint32_t mepc = riscv_read_csr(mepc);
    mepc += 4;
    riscv_write_csr(mepc, mepc);

    uint32_t arg;
    asm volatile("mv %0,a0" : "=r"(arg));
    // Invoke the RTOS syscall handler
    syscallHandler(arg);
}
