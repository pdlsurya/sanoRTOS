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
#include "interrupts.h"
#include "hal/apm_ll.h"
#include "usb_serial.h"

/*RTOS tick handler function*/
extern void tickHandler(void);

volatile privilegeModesType privilegeMode = MACHINE_MODE;

void IRAM_ATTR syscallHandler(uint32_t sysCode)
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
        RV_SET_CSR(mstatus, MSTATUS_MPP);
        break;

    case EXIT_PRIVILEGED_MODE:
        RV_CLEAR_CSR(mstatus, MSTATUS_MPP);
        break;

    case GET_PRIVILEGE_MODE:
        privilegeMode = ((RV_READ_CSR(mstatus) & MSTATUS_MPP) != 0) ? MACHINE_MODE : USER_MODE;
        break;

    default:
        break;
    }
}

/**
 * @brief Configure platform specific interrupts and tick timer.
 *
 */
static inline void portConfig()
{
    /*Replace this with vendor specific machine software interrupt enable api and
    machine timer interrupt callback api */

    /*Enable machine software interrupt*/
    msi_enable();

    /*initialize mtimer to generate interrupt every 1 ms*/
    mtimer_cb_init_t mtimer_cb_init = {0};
    mtimer_cb_init.period_ticks = TIMER_TICKS_PER_RTOS_TICK;
    mtimer_cb_init.cb_function = tickHandler;
    mtimer_callback_init(&mtimer_cb_init);
}

/**
 * @brief Configure esp32 specific Aceess Permission Management (APM) to allow user mode access to peripherals and memory regions.
 *
 */
static inline void apmConfigure()
{
    apm_ll_apm_ctrl_set_region_start_address(HP_APM_CTRL, 0, SOC_PERIPHERAL_LOW);
    apm_ll_apm_ctrl_set_region_end_address(HP_APM_CTRL, 0, SOC_PERIPHERAL_HIGH);

    apm_ll_apm_ctrl_sec_mode_region_attr_config(HP_APM_CTRL, 0, APM_LL_SECURE_MODE_REE0, (HP_APM_REGION0_R0_PMS_W | HP_APM_REGION0_R0_PMS_R));
    apm_ll_apm_ctrl_region_filter_enable(HP_APM_CTRL, 0, true);
}

/**
 * @brief Configure Physical Memory Protection (PMP) to allow user mode access to peripherals and memory regions.
 *
 */
void pmpConfigure()
{
    /*NAPOT pmpaddr value= (start_address >> 2) | (size - 1) >> 3 */
    // FLASH (4MB @0x42000000, Read/Execute, NAPOT)
    uint32_t pmpaddr0 = (SOC_IROM_LOW >> 2) | ((4 * 1024 * 1024 - 1) >> 3);
    uint8_t pmp0cfg = (PMP_R | PMP_X | PMP_NAPOT | PMP_L);

    // RAM (512KB@0x40800000, Read/Write/Execute, NAPOT)
    uint32_t pmpaddr1 = (SOC_DRAM_LOW >> 2) | ((512 * 1024 - 1) >> 3);
    uint8_t pmp1cfg = (PMP_R | PMP_W | PMP_X | PMP_NAPOT | PMP_L);

    // PERIPHERALS (832KB @ 0x60000000, Read/Write, TOR)
    uint32_t pmpaddr2 = (SOC_PERIPHERAL_LOW >> 2);  // Start address(Inclusive)
    uint32_t pmpaddr3 = (SOC_PERIPHERAL_HIGH >> 2); // End address(Exclusive)

    // preceding pmpcfg[pmp2cfg in this case] entry is neglected for TOR.
    uint8_t pmp3cfg = (PMP_R | PMP_W | PMP_TOR | PMP_L);

    // CPU Subsystem (0x20000000, Read/Write, NAPOT)
    uint32_t pmpaddr4 = (SOC_CPU_SUBSYSTEM_LOW >> 2) | ((64 * 1024 - 1) >> 3);
    uint8_t pmp4cfg = (PMP_R | PMP_W | PMP_NAPOT | PMP_L);

    // Write PMPADDR registers
    RV_WRITE_CSR(pmpaddr0, pmpaddr0);
    RV_WRITE_CSR(pmpaddr1, pmpaddr1);
    RV_WRITE_CSR(pmpaddr2, pmpaddr2);
    RV_WRITE_CSR(pmpaddr3, pmpaddr3);
    RV_WRITE_CSR(pmpaddr4, pmpaddr4);

    // Pack and write to PMPCFG0 register
    uint32_t pmpcfg0 = ((pmp3cfg << 24) | (pmp1cfg << 8) | pmp0cfg);

    RV_WRITE_CSR(pmpcfg0, pmpcfg0);

    RV_WRITE_CSR(pmpcfg1, pmp4cfg);
}

/**
 * @brief Start the scheduler by jumping to the first task.
 *
 */
void portSchedulerStart()
{

    taskQueueType *pReadyQueue = getReadyQueue();

    /*Get the highest priority ready task from ready Queue*/
    currentTask[PORT_CORE_ID()] = TASK_GET_FROM_READY_QUEUE(pReadyQueue);
    taskSetCurrent(currentTask[PORT_CORE_ID()]);

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

    pmpConfigure();

    apmConfigure();

    /* Clear MPP bits of mstatus to switch to user mode(U-mode)*/
    RV_CLEAR_CSR(mstatus, MSTATUS_MPP);

    /* MPIE is set to 1 to restore interrupt enable state after the transition.*/
    RV_SET_CSR(mstatus, MSTATUS_MPIE);

    /* Set up the user-mode entry point*/
    RV_WRITE_CSR(mepc, currentTask[PORT_CORE_ID()]->entry);

    // Execute mret to switch to U-mode
    asm volatile("mret");

#else
    /*If user mode not enabled, jump directly to the first task*/
    currentTask[PORT_CORE_ID()]->entry(currentTask[PORT_CORE_ID()]->params);
#endif
}
