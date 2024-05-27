/**
 * @file scheduler.h
 * @author Surya Poudel
 * @brief
 * @version 0.1
 * @date 2024-04-09
 *
 * @copyright Copyright (c) 2024
 *
 */
#ifndef __SANO_RTOS_SCHEDULER_H
#define __SANO_RTOS_SCHEDULER_H

#include "osConfig.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef PLATFORM_STM32
#define SYSTICK_HANDLER osSysTick_Handler
/*For STM32 SoCs, SysTick timer time initialized during ClockConfig stage;
 Hence, we dont need it re-initialize SysTick timer for STM32 platform.*/
#define SYSTICK_CONFIG()
    void osSysTick_Handler();
#else
#define SYSTICK_HANDLER SysTick_Handler
#define SYSTICK_CONFIG() SysTick_Config(OS_INTERVAL_CPU_TICKS)
#endif

    void schedulerStart();

    void taskYield();

#ifdef __cplusplus
}
#endif

#endif
