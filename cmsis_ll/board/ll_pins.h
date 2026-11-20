#ifndef FX9A_BOARD_LL_PINS_H
#define FX9A_BOARD_LL_PINS_H
#include "stm32f1xx_ll_gpio.h"

/* STM32F1 LL's Pin field is an encoded selection mask, not the low 16-bit
 * physical pin bitmask used by GPIO IDR/BSRR. Convert board signal masks
 * explicitly, including PB8..PB15, without any StdPeriph API.
 */
static inline uint32_t fx9a_ll_pins(uint32_t physical_mask)
{
    static const uint32_t encoded[16] = {
        LL_GPIO_PIN_0, LL_GPIO_PIN_1, LL_GPIO_PIN_2, LL_GPIO_PIN_3,
        LL_GPIO_PIN_4, LL_GPIO_PIN_5, LL_GPIO_PIN_6, LL_GPIO_PIN_7,
        LL_GPIO_PIN_8, LL_GPIO_PIN_9, LL_GPIO_PIN_10, LL_GPIO_PIN_11,
        LL_GPIO_PIN_12, LL_GPIO_PIN_13, LL_GPIO_PIN_14, LL_GPIO_PIN_15
    };
    uint32_t pins = 0u;
    for (unsigned bit = 0; bit < 16; ++bit)
        if (physical_mask & (1u << bit)) pins |= encoded[bit];
    return pins;
}
#endif
