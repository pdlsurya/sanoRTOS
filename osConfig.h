#ifndef __MY_RTOS_H
#define __MY_RTOS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#if defined(NRF52840_XXAA)
#include "nrf52840.h"
#elif defined(STM32F401xC)
#include "stm32f4xx_hal.h"
#endif

#include "cmsis_gcc.h"
#include "core_cm4.h"


#define TASKS_RUN_PRIV 1 // macro to set whether tasks should run in privileged mode.

#define MUTEX_USE_PRIORITY_INHERITANCE 1

#define OS_TICK_INTERVAL_US 1000 // Generate SysTick interrupt every 1ms.

#define MS_TO_CPU_TICKS(ms) ((uint32_t)((uint64_t)ms * SystemCoreClock / 1000))
#define US_TO_CPU_TICKS(us) ((uint32_t)((uint64_t)us * SystemCoreClock / 1000000))

/*Number to CPU cycles between two OS Ticks*/
#define OS_INTERVAL_CPU_TICKS US_TO_CPU_TICKS(OS_TICK_INTERVAL_US)

#define US_TO_OS_TICKS(us) ((uint32_t)(US_TO_CPU_TICKS(us) / OS_INTERVAL_CPU_TICKS))
#define MS_TO_OS_TICKS(ms) ((uint32_t)(MS_TO_CPU_TICKS(ms) / OS_INTERVAL_CPU_TICKS))


#endif
