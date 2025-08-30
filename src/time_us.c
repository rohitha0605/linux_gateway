#include <stdint.h>

/* ===== Option A: FreeRTOS ticks -> microseconds ===== */
#if defined(__has_include)
#  if __has_include("FreeRTOS.h")
#    include "FreeRTOS.h"
#    include "task.h"
#    define HAS_FREERTOS 1
#  endif
#endif

#if HAS_FREERTOS
uint32_t now_us(void) {
    const uint32_t tick_us = 1000000u / configTICK_RATE_HZ;
    return (uint32_t)xTaskGetTickCount() * tick_us;
}
#else
/* ===== Option B: DWT cycle counter (adjust core header if needed) ===== */
#include "core_cm7.h"
extern uint32_t SystemCoreClock;
uint32_t now_us(void) {
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
    uint32_t cycles = DWT->CYCCNT;
    return (uint32_t)((uint64_t)cycles * 1000000ull / (uint64_t)SystemCoreClock);
}
#endif
