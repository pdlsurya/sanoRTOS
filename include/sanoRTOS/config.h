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

#ifndef __SANO_RTOS_CONFIG_H
#define __SANO_RTOS_CONFIG_H

#include "sanoRTOS/retCodes.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define CONFIG_SMP 1 // Configure Symmetric Multi Processing

#define CONFIG_MUTEX_USE_PRIORITY_INHERITANCE 1 // Configure mutex priority inheritance

#define CONFIG_TASK_USER_MODE 0 // Configure whether tasks should run in privileged mode.

#define CONFIG_TICK_INTERVAL_US 100 // Configure SysTick to generate interrupt every 1ms.

#ifdef __cplusplus
}
#endif

#endif
