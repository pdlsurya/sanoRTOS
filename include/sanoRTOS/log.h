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

#ifndef __SANORTOS_LOG_H
#define __SANORTOS_LOG_H

#include <stdio.h>
#include <stdarg.h>
#include "sanoRTOS/port.h"
#include "sanoRTOS/spinLock.h"
#include "sanoRTOS/config.h"

#define LOG_MODULE_DEFINE(module_name) static const char *tag = #module_name

#if CONFIG_LOG_COLOR
// ANSI escape prefix
#define ANSI_ESC "\x1b["
// Reset
#define ANSI_RESET ANSI_ESC "0m"
// Regular colors
#define ANSI_BLACK ANSI_ESC "30m"
#define ANSI_RED ANSI_ESC "31m"
#define ANSI_GREEN ANSI_ESC "32m"
#define ANSI_YELLOW ANSI_ESC "33m"
#define ANSI_BLUE ANSI_ESC "34m"
#define ANSI_MAGENTA ANSI_ESC "35m"
#define ANSI_CYAN ANSI_ESC "36m"
#define ANSI_WHITE ANSI_ESC "37m"
// Bright versions
#define ANSI_BRIGHT_BLACK ANSI_ESC "90m"
#define ANSI_BRIGHT_RED ANSI_ESC "91m"
#define ANSI_BRIGHT_GREEN ANSI_ESC "92m"
#define ANSI_BRIGHT_YELLOW ANSI_ESC "93m"
#define ANSI_BRIGHT_BLUE ANSI_ESC "94m"
#define ANSI_BRIGHT_MAGENTA ANSI_ESC "95m"
#define ANSI_BRIGHT_CYAN ANSI_ESC "96m"
#define ANSI_BRIGHT_WHITE ANSI_ESC "97m"
// Bold (optional)
#define ANSI_BOLD ANSI_ESC "1m"

#else
#define ANSI_RESET
#define ANSI_ESC
#define ANSI_BLACK
#define ANSI_RED
#define ANSI_GREEN
#define ANSI_YELLOW
#define ANSI_BLUE
#define ANSI_MAGENTA
#define ANSI_CYAN
#define ANSI_WHITE
#define ANSI_BRIGHT_BLACK
#define ANSI_BRIGHT_RED
#define ANSI_BRIGHT_GREEN
#define ANSI_BRIGHT_YELLOW
#define ANSI_BRIGHT_BLUE
#define ANSI_BRIGHT_MAGENTA
#define ANSI_BRIGHT_CYAN
#define ANSI_BRIGHT_WHITE
#define ANSI_BOLD

#endif
#if CONFIG_LOG

#define LOG_PRINT(fmt, ...) log_internal_printf(fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) log_internal_printf(ANSI_GREEN "[INF] [%s]: " fmt ANSI_RESET "\n", tag, ##__VA_ARGS__)

#define LOG_WARNING(fmt, ...) log_internal_printf(ANSI_BRIGHT_YELLOW "[WRN] [%s]: " fmt ANSI_RESET "\n", tag, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) log_internal_printf(ANSI_RED "[ERR] [%s]: " fmt ANSI_RESET "\n", tag, ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...) log_internal_printf(ANSI_CYAN "[DBG] [%s]: " fmt ANSI_RESET "\n", tag, ##__VA_ARGS__)

#else

#define LOG_PRINT(fmt, ...)

#define LOG_INFO(fmt, ...)

#define LOG_WARNING(fmt, ...)

#define LOG_ERROR(fmt, ...)

#define LOG_DEBUG(fmt, ...)

#endif

int log_internal_printf(const char *format, ...);

#endif // __SANORTOS_LOG_H