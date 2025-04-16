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
 * @brief Configure Physical Memory Protection (PMP) to allow user mode access to peripherals and memory regions.
 *
 */
/*
void pmpConfigure()
{
    //NAPOT pmpaddr value= (start_address >> 2) | (size - 1) >> 3
    // FLASH (4MB @0x42000000, Read/Execute, NAPOT)
    uint32_t pmpaddr0 = (SOC_IROM_LOW >> 2) | ((4 * 1024 * 1024 - 1) >> 3);
    uint8_t pmp0cfg = (PMP_R | PMP_X | PMP_NAPOT | PMP_L);

    // RAM (520KB@0x20000000, Read/Write/Execute, NAPOT)
    uint32_t pmpaddr1 = (SOC_DRAM_LOW >> 2) | ((520 * 1024 - 1) >> 3);
    uint8_t pmp1cfg = (PMP_R | PMP_W | PMP_X | PMP_NAPOT | PMP_L);

    // PERIPHERALS (832KB @ 0x40000000, Read/Write, TOR)
    uint32_t pmpaddr2 = (SOC_PERIPHERAL_LOW >> 2);  // Start address(Inclusive)
    uint32_t pmpaddr3 = (SOC_PERIPHERAL_HIGH >> 2); // End address(Exclusive)

    // preceding pmpcfg[pmp2cfg in this case] entry is neglected for TOR.
    uint8_t pmp3cfg = (PMP_R | PMP_W | PMP_TOR | PMP_L);

    // CPU Subsystem (0x20000000, Read/Write, NAPOT)
    uint32_t pmpaddr4 = (SOC_CPU_SUBSYSTEM_LOW >> 2) | ((64 * 1024 - 1) >> 3);
    uint8_t pmp4cfg = (PMP_R | PMP_W | PMP_NAPOT | PMP_L);

    // Write PMPADDR registers
    riscv_write_csr(pmpaddr0, pmpaddr0);
    riscv_write_csr(pmpaddr1, pmpaddr1);
    riscv_write_csr(pmpaddr2, pmpaddr2);
    riscv_write_csr(pmpaddr3, pmpaddr3);
    riscv_write_csr(pmpaddr4, pmpaddr4);

    // Pack and write to PMPCFG0 register
    uint32_t pmpcfg0 = ((pmp3cfg << 24) | (pmp1cfg << 8) | pmp0cfg);

    riscv_write_csr(pmpcfg0, pmpcfg0);

    riscv_write_csr(pmpcfg1, pmp4cfg);
}
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
