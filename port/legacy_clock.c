#include <stdint.h>

/* Compatibility symbol expected by the 2009 STM32 Standard Peripheral
 * Library. The custom Mbed target configures the AHB clock to 72 MHz. */
const uint32_t SystemFrequency_AHBClk = 72000000U;
