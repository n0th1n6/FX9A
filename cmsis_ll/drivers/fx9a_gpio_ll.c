/* FX-9A GPIO register-level driver, STM32F1.
 *
 * Transitional compatibility API: existing BSP callers retain the old
 * function signatures while the underlying StdPeriph GPIO implementation
 * is removed from the standalone build. Uses STM32F1 CRL/CRH, BSRR/BRR,
 * AFIO MAPR/EXTICR registers directly (LL-style).
 *
 * Pin mode layout matches STM32F1 reference manual; this file does not
 * use or link any functions from stm32f10x_gpio.c.
 */
#include "fx9a_legacy_types.h"
#include "stm32f1xx_ll_gpio.h"

void GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config)
{
    const uint32_t mode = (uint32_t)config->GPIO_Mode;
    const uint32_t speed = (uint32_t)config->GPIO_Speed & 3u;
    const uint32_t cnf_mode = (mode & 0x0fu) | ((mode & 0x10u) ? speed : 0u);

    /* For STM32F1 pull input, CRL/CRH selects input pull (CNF=10)
     * and the output latch bit selects pull-up or pull-down.
     */
    if (config->GPIO_Mode == GPIO_Mode_IPD)
        port->BRR = config->GPIO_Pin;
    else if (config->GPIO_Mode == GPIO_Mode_IPU)
        port->BSRR = config->GPIO_Pin;

    uint32_t low = port->CRL;
    uint32_t high = port->CRH;
    for (uint32_t pin = 0; pin < 16; ++pin) {
        if (!(config->GPIO_Pin & (1u << pin)))
            continue;
        uint32_t shift = (pin & 7u) * 4u;
        if (pin < 8u)
            low = (low & ~(0xfu << shift)) | (cnf_mode << shift);
        else
            high = (high & ~(0xfu << shift)) | (cnf_mode << shift);
    }
    port->CRL = low;
    port->CRH = high;
}

void GPIO_SetBits(GPIO_TypeDef *port, uint16_t pins)
{
    /* LL_GPIO_PIN_x encodes the pin mask shifted by GPIO_PIN_MASK_POS;
     * original StdPeriph GPIO_Pin_x is an unshifted 16-bit mask. */
    const uint32_t llmask = ((uint32_t)pins << GPIO_PIN_MASK_POS) | pins;
    LL_GPIO_SetOutputPin(port, llmask);
}

void GPIO_ResetBits(GPIO_TypeDef *port, uint16_t pins)
{
    const uint32_t llmask = ((uint32_t)pins << GPIO_PIN_MASK_POS) | pins;
    LL_GPIO_ResetOutputPin(port, llmask);
}

uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef *port, uint16_t pins)
{
    /* Legacy ReadInputDataBit checks whether ANY masked input is high;
     * LL_GPIO_IsInputPinSet requires ALL requested bits high. */
    return (LL_GPIO_ReadInputPort(port) & pins) ? (uint8_t)Bit_SET : (uint8_t)Bit_RESET;
}

uint16_t GPIO_ReadInputData(GPIO_TypeDef *port)
{
    return (uint16_t)LL_GPIO_ReadInputPort(port);
}

void GPIO_PinRemapConfig(uint32_t remap, FunctionalState enabled)
{
    /* The FX-9A currently requests only SWJ JTAG disable while preserving SWD.
     * General-purpose remapping must be added explicitly when needed; do not
     * silently modify unknown MAPR bits.
     */
    if (remap == GPIO_Remap_SWJ_JTAGDisable) {
        uint32_t mapr = AFIO->MAPR;
        mapr &= ~(7u << 24); /* SWJ_CFG */
        if (enabled != DISABLE)
            mapr |= 2u << 24; /* JTAG off, SWD remains on */
        AFIO->MAPR = mapr;
    }
}

void GPIO_EXTILineConfig(uint8_t port, uint8_t pin)
{
    const uint32_t index = pin >> 2;
    const uint32_t shift = (pin & 3u) * 4u;
    uint32_t reg = AFIO->EXTICR[index];
    reg &= ~(0xfu << shift);
    reg |= ((uint32_t)port & 0xfu) << shift;
    AFIO->EXTICR[index] = reg;
}
