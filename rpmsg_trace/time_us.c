#include <stdint.h>
#include "core_cm7.h"  /* adjust if not CM7 */
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
