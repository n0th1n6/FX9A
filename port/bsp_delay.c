#include "bsp_delay.h"

volatile uint8_t fac_us = 0;

void delay_init(uint8_t clk)
{
    if (clk > 16) {
        fac_us = (16 - 4) / 4;
    } else if (clk > 4) {
        fac_us = (clk - 4) / 4;
    } else {
        fac_us = 1;
    }
}

void delay_us(uint32_t nus)
{
    /* Keep the checked-in IAR release's exact inner instruction sequence.
     * Straight C with uint16_t adds UXTH; uint32_t lets GCC remove CMP. Either
     * changes every bit-banged I2C, DDS, and OLED timing interval. */
    while (nus--) {
        uint32_t count = 72;
        __asm volatile(
            "1:\n"
            "nop\n"
            "subs %[count], %[count], #1\n"
            "cmp %[count], #0\n"
            "bne 1b\n"
            : [count] "+r"(count)
            :
            : "cc");
    }
}

void delay_ms(uint32_t nms)
{
    delay_us(nms * 1000);
}
