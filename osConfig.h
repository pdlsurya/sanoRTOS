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

#ifndef __OS_CONFIG_H
#define __OS_CONFIG_H

// #define PLATFORM_STM32

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#if defined(NRF52840_XXAA)
#include "nrf52840.h"
#elif defined(PLATFORM_STM32)
#include "stm32f4xx_hal.h"
#endif

#include "cmsis_gcc.h"
#include "core_cm4.h"

#include "retCodes.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define SYSTICK_PRIORITY 0xff // Priority of SysTick Timer.

#define TASK_RUN_PRIVILEGED 1 // macro to set whether tasks should run in privileged mode.

#define MUTEX_USE_PRIORITY_INHERITANCE 1

#define OS_TICK_INTERVAL_US 1000 // Generate SysTick interrupt every 1ms.

#define MS_TO_CPU_TICKS(ms) ((uint32_t)((uint64_t)ms * SystemCoreClock / 1000))
#define US_TO_CPU_TICKS(us) ((uint32_t)((uint64_t)us * SystemCoreClock / 1000000))

/*Number to CPU cycles between two OS Ticks*/
#define OS_INTERVAL_CPU_TICKS US_TO_CPU_TICKS(OS_TICK_INTERVAL_US)

#define US_TO_OS_TICKS(us) ((uint32_t)(US_TO_CPU_TICKS(us) / OS_INTERVAL_CPU_TICKS))
#define MS_TO_OS_TICKS(ms) ((uint32_t)(MS_TO_CPU_TICKS(ms) / OS_INTERVAL_CPU_TICKS))

#ifdef __cplusplus
}
#endif

#endif
