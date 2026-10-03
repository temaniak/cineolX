// QSPI bootstrap: relocate code, vectors and hot DSP before libDaisy initialization.
// The existing Daisy bootloader receives a QSPI reset vector, unchanged.
#include "stm32h7xx.h"
#define Reset_Handler HallRamResetHandler
#define main HallStartupMain
#include "startup_stm32h750xx.c"
#undef main
#undef Reset_Handler

extern int main(void);
int HallStartupMain(void)
{
    __set_BASEPRI(0);
    __enable_irq(); // C initialization and RAM interrupt vectors are ready.
    return main();
}
extern uint32_t _hall_runtime_load, _hall_runtime_start, _hall_runtime_end;
extern uint32_t _hall_vector_load, _hall_vector_start, _hall_vector_end;
extern uint32_t _hall_itcm_load, _hall_itcm_start, _hall_itcm_end;

// Do not call memcpy before its RAM code has been copied.
static __attribute__((always_inline)) inline void CopyBootWords(
    const uint32_t* source, uint32_t* start, uint32_t* end, int cacheable)
{
    volatile uint32_t* dst = start;
    const volatile uint32_t* src = source;
    while(dst < end) *dst++ = *src++;
    if(cacheable && (SCB->CCR & SCB_CCR_DC_Msk))
    {
        uintptr_t lo = (uintptr_t)start & ~(uintptr_t)31;
        uintptr_t hi = ((uintptr_t)end + 31) & ~(uintptr_t)31;
        SCB_CleanDCache_by_Addr((uint32_t*)lo, (int32_t)(hi - lo));
    }
}
void __attribute__((section(".qspi_boot_text"), noreturn, noinline)) HallQspiBootReset(void)
{
    __disable_irq();
    RCC->AHB2ENR |= RCC_AHB2ENR_SRAM1EN | RCC_AHB2ENR_SRAM2EN | RCC_AHB2ENR_SRAM3EN;
    (void)RCC->AHB2ENR;
    SCB->ITCMCR |= 1u;
    __DSB(); __ISB();
    CopyBootWords(&_hall_runtime_load, &_hall_runtime_start, &_hall_runtime_end, 1);
    CopyBootWords(&_hall_itcm_load, &_hall_itcm_start, &_hall_itcm_end, 0);
    CopyBootWords(&_hall_vector_load, &_hall_vector_start, &_hall_vector_end, 1);
    SCB_InvalidateICache();
    SCB->VTOR = (uint32_t)&_hall_vector_start;
    __DSB(); __ISB();
    HallRamResetHandler();
}
// Cover a tick/fault arriving before RAM vectors are installed.
static void __attribute__((section(".qspi_boot_text"))) HallBootSysTick(void) {}
static void __attribute__((section(".qspi_boot_text"), noreturn)) HallBootFault(void)
{
    __disable_irq();
    for(;;) __NOP();
}
__attribute__((section(".qspi_boot_vector"), used))
void* const hall_boot_vector[0xa6] = {
    [0] = &_estack,
    [1] = &HallQspiBootReset,
    [2 ... 14] = &HallBootFault,
    [15] = &HallBootSysTick,
    [16 ... 165] = &HallBootFault,
};
