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
#include "sanoRTOS/port_common.h"
#include "cmsis_gcc.h"
#include "sanoRTOS/config.h"
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define CPU_FREQ SystemCoreClock

/*STM32CubeIDE already defines SysTick_Handler. We define osSysTick_Handler here and invoke it from SysTick_Handler*/
#define SYSTICK_HANDLER osSysTick_Handler

/*For STM32 SoCs, SysTick timer is initialized during ClockConfig stage.
 Hence, we dont need to re-initialize SysTick timer for STM32 platform.*/
#define SYSTICK_CONFIG() ((void)0)

    /**
     * @brief Configure platform specific core components and
     * start the RTOS scheduler by jumping to the first task.
     *
     * @param pTask  Pointer to the first task to be executed
     */
    void portSchedulerStart(taskHandleType *pTask);

#ifdef __cplusplus
}
#endif

#endif
