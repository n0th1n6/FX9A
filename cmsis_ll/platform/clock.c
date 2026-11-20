/* Clock and early board power hold for minimal standalone bring-up.
 * Uses CMSIS STM32F10x register declarations directly. STM32 LL driver
 * integration is a later milestone; no legacy StdPeriph functions are used.
 */
#include <stdint.h>
#include "stm32f10x.h"

volatile uint32_t fx9a_system_clock_hz = 8000000u;
volatile uint32_t fx9a_clock_status = 0u;

/* Known original FX-9A power latch: PC5 driven high. Other GPIO remains
 * in reset state; no TX, PA, CW, microphone, DDS, or filter pins are driven.
 */
void fx9a_early_power_hold(void)
{
    RCC->APB2ENR |= (1u << 4); /* GPIOCEN */
    GPIOC->BSRR = (1u << 5);
    GPIOC->CRL = (GPIOC->CRL & ~(0xfu << 20)) | (0x2u << 20);
}

void fx9a_clock_init(void)
{
    /* HSI is the hardware reset clock. Set 2 flash wait states and prefetch
     * before switching to the 72 MHz PLL clock.
     */
    FLASH->ACR = (FLASH->ACR & ~0x7u) | 0x12u; /* PRFTBE + LATENCY=2 */
    RCC->CR |= (1u << 16); /* HSEON */

    uint32_t timeout = 2000000u;
    while (!(RCC->CR & (1u << 17)) && --timeout) { }
    if (!timeout) {
        fx9a_clock_status = 1u; /* HSE failed, stay on HSI at 8 MHz. */
        return;
    }

    /* AHB /1, APB1 /2, APB2 /1, PLL source HSE /2 *9:
     * 16 MHz / 2 * 9 = 72 MHz.
     */
    uint32_t cfgr = RCC->CFGR;
    cfgr &= ~((0xfu << 4) | (0x7u << 8) | (0x7u << 11) |
              (0x3u << 16) | (0x1u << 17) | (0xfu << 18));
    cfgr |= (0x4u << 8) | (1u << 16) | (1u << 17) | (0x7u << 18);
    RCC->CFGR = cfgr;

    RCC->CR |= (1u << 24); /* PLLON */
    timeout = 2000000u;
    while (!(RCC->CR & (1u << 25)) && --timeout) { }
    if (!timeout) {
        fx9a_clock_status = 2u; /* PLL failed, stay on HSI at 8 MHz. */
        return;
    }

    RCC->CFGR = (RCC->CFGR & ~3u) | 2u; /* SW=PLL */
    timeout = 2000000u;
    while (((RCC->CFGR >> 2) & 3u) != 2u && --timeout) { }
    if (!timeout) {
        fx9a_clock_status = 3u; /* Switch failed: still running HSI. */
        return;
    }

    fx9a_system_clock_hz = 72000000u;
    fx9a_clock_status = 0x72u; /* Distinct debugger-visible success code. */
}
