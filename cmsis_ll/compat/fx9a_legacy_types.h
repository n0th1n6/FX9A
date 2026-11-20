#ifndef FX9A_LEGACY_PERIPHERAL_API_H
#define FX9A_LEGACY_PERIPHERAL_API_H
/*
 * FX-9A application-facing compatibility types, not STM32 StdPeriph.
 * Device registers, IRQs and CMSIS primitives come ONLY from ST CMSIS F1.
 * The signatures will disappear when the BSP callers move to LL directly.
 * Numeric bitfield constants match the original FX-9A firmware ABI.
 */
#include "stm32f10x.h"
#include <stdint.h>

/* GPIO */
typedef uint8_t GPIOSpeed_TypeDef, GPIOMode_TypeDef;
typedef struct {
    uint16_t GPIO_Pin;
    GPIOSpeed_TypeDef GPIO_Speed;
    GPIOMode_TypeDef GPIO_Mode;
} GPIO_InitTypeDef;
typedef enum { Bit_RESET = 0, Bit_SET = 1 } BitAction;
#define GPIO_Speed_10MHz 1u
#define GPIO_Speed_2MHz  2u
#define GPIO_Speed_50MHz 3u
#define GPIO_Mode_AIN 0x00u
#define GPIO_Mode_IN_FLOATING 0x04u
#define GPIO_Mode_IPD 0x28u
#define GPIO_Mode_IPU 0x48u
#define GPIO_Mode_Out_OD 0x14u
#define GPIO_Mode_Out_PP 0x10u
#define GPIO_Mode_AF_OD 0x1cu
#define GPIO_Mode_AF_PP 0x18u
#define GPIO_Remap_SWJ_JTAGDisable 0x00300200u
#define GPIO_Pin_All 0xffffu
#define GPIO_Pin_0 (1u << 0)
#define GPIO_Pin_1 (1u << 1)
#define GPIO_Pin_2 (1u << 2)
#define GPIO_Pin_3 (1u << 3)
#define GPIO_Pin_4 (1u << 4)
#define GPIO_Pin_5 (1u << 5)
#define GPIO_Pin_6 (1u << 6)
#define GPIO_Pin_7 (1u << 7)
#define GPIO_Pin_8 (1u << 8)
#define GPIO_Pin_9 (1u << 9)
#define GPIO_Pin_10 (1u << 10)
#define GPIO_Pin_11 (1u << 11)
#define GPIO_Pin_12 (1u << 12)
#define GPIO_Pin_13 (1u << 13)
#define GPIO_Pin_14 (1u << 14)
#define GPIO_Pin_15 (1u << 15)
#define GPIO_PortSourceGPIOA 0u
#define GPIO_PortSourceGPIOB 1u
#define GPIO_PortSourceGPIOC 2u
#define GPIO_PortSourceGPIOD 3u
#define GPIO_PortSourceGPIOE 4u
#define GPIO_PortSourceGPIOF 5u
#define GPIO_PortSourceGPIOG 6u
#define GPIO_PinSource0 0u
#define GPIO_PinSource1 1u
#define GPIO_PinSource2 2u
#define GPIO_PinSource3 3u
#define GPIO_PinSource4 4u
#define GPIO_PinSource5 5u
#define GPIO_PinSource6 6u
#define GPIO_PinSource7 7u
#define GPIO_PinSource8 8u
#define GPIO_PinSource9 9u
#define GPIO_PinSource10 10u
#define GPIO_PinSource11 11u
#define GPIO_PinSource12 12u
#define GPIO_PinSource13 13u
#define GPIO_PinSource14 14u
#define GPIO_PinSource15 15u
void GPIO_Init(GPIO_TypeDef *, GPIO_InitTypeDef *);
void GPIO_SetBits(GPIO_TypeDef *, uint16_t);
void GPIO_ResetBits(GPIO_TypeDef *, uint16_t);
uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef *, uint16_t);
uint16_t GPIO_ReadInputData(GPIO_TypeDef *);
void GPIO_PinRemapConfig(uint32_t, FunctionalState);
void GPIO_EXTILineConfig(uint8_t, uint8_t);

/* RCC: all masks are the STM32F1 RCC register bit positions. */
#define RCC_AHBPeriph_DMA1 0x00000001u
#define RCC_APB2Periph_AFIO 0x00000001u
#define RCC_APB2Periph_GPIOA 0x00000004u
#define RCC_APB2Periph_GPIOB 0x00000008u
#define RCC_APB2Periph_GPIOC 0x00000010u
#define RCC_APB2Periph_GPIOD 0x00000020u
#define RCC_APB2Periph_GPIOE 0x00000040u
#define RCC_APB2Periph_ADC1 0x00000200u
#define RCC_APB2Periph_USART1 0x00004000u
#define RCC_APB1Periph_TIM2 0x00000001u
#define RCC_APB1Periph_TIM3 0x00000002u
#define RCC_APB1Periph_TIM4 0x00000004u
typedef struct {
    uint32_t SYSCLK_Frequency;
    uint32_t HCLK_Frequency;
    uint32_t PCLK1_Frequency;
    uint32_t PCLK2_Frequency;
    uint32_t ADCCLK_Frequency;
} RCC_ClocksTypeDef;
void RCC_AHBPeriphClockCmd(uint32_t, FunctionalState);
void RCC_APB1PeriphClockCmd(uint32_t, FunctionalState);
void RCC_APB2PeriphClockCmd(uint32_t, FunctionalState);
void RCC_APB1PeriphResetCmd(uint32_t, FunctionalState);
void RCC_APB2PeriphResetCmd(uint32_t, FunctionalState);
void RCC_GetClocksFreq(RCC_ClocksTypeDef *);

/* NVIC/EXTI */
#define NVIC_PriorityGroup_2 0x500u
typedef struct {
    uint8_t NVIC_IRQChannel;
    uint8_t NVIC_IRQChannelPreemptionPriority;
    uint8_t NVIC_IRQChannelSubPriority;
    FunctionalState NVIC_IRQChannelCmd;
} NVIC_InitTypeDef;
void NVIC_PriorityGroupConfig(uint32_t);
void NVIC_Init(NVIC_InitTypeDef *);

typedef uint8_t EXTIMode_TypeDef, EXTITrigger_TypeDef;
#define EXTI_Mode_Interrupt 0x00u
#define EXTI_Mode_Event 0x04u
#define EXTI_Trigger_Rising 0x08u
#define EXTI_Trigger_Falling 0x0cu
#define EXTI_Trigger_Rising_Falling 0x10u
#define EXTI_Line0 (1u << 0)
#define EXTI_Line1 (1u << 1)
#define EXTI_Line2 (1u << 2)
#define EXTI_Line3 (1u << 3)
#define EXTI_Line4 (1u << 4)
#define EXTI_Line5 (1u << 5)
#define EXTI_Line6 (1u << 6)
#define EXTI_Line7 (1u << 7)
#define EXTI_Line8 (1u << 8)
#define EXTI_Line9 (1u << 9)
#define EXTI_Line10 (1u << 10)
#define EXTI_Line11 (1u << 11)
#define EXTI_Line12 (1u << 12)
#define EXTI_Line13 (1u << 13)
#define EXTI_Line14 (1u << 14)
#define EXTI_Line15 (1u << 15)
#define EXTI_Line16 (1u << 16)
#define EXTI_Line17 (1u << 17)
#define EXTI_Line18 (1u << 18)
#define EXTI_Line19 (1u << 19)
typedef struct {
    uint32_t EXTI_Line;
    EXTIMode_TypeDef EXTI_Mode;
    EXTITrigger_TypeDef EXTI_Trigger;
    FunctionalState EXTI_LineCmd;
} EXTI_InitTypeDef;
void EXTI_Init(EXTI_InitTypeDef *);
ITStatus EXTI_GetITStatus(uint32_t);
void EXTI_ClearITPendingBit(uint32_t);

/* Timer */
#define TIM_CKD_DIV1 0x0000u
#define TIM_CounterMode_Up 0x0000u
#define TIM_OCMode_Timing 0x0000u
#define TIM_OCMode_PWM1 0x0060u
#define TIM_OCPolarity_High 0x0000u
#define TIM_OCNPolarity_High 0x0000u
#define TIM_OutputState_Enable 0x0001u
#define TIM_OutputState_Disable 0x0000u
#define TIM_OutputNState_Disable 0x0000u
#define TIM_OCIdleState_Reset 0x0000u
#define TIM_OCNIdleState_Reset 0x0000u
#define TIM_OCPreload_Enable 0x0008u
#define TIM_IT_Update 0x0001u
#define TIM_FLAG_Update 0x0001u
typedef struct {
    uint16_t TIM_Prescaler;
    uint16_t TIM_CounterMode;
    uint16_t TIM_Period;
    uint16_t TIM_ClockDivision;
    uint8_t TIM_RepetitionCounter;
} TIM_TimeBaseInitTypeDef;
typedef struct {
    uint16_t TIM_OCMode, TIM_OutputState, TIM_OutputNState;
    uint16_t TIM_Pulse, TIM_OCPolarity, TIM_OCNPolarity;
    uint16_t TIM_OCIdleState, TIM_OCNIdleState;
} TIM_OCInitTypeDef;
void TIM_DeInit(TIM_TypeDef *);
void TIM_TimeBaseInit(TIM_TypeDef *, TIM_TimeBaseInitTypeDef *);
void TIM_OCStructInit(TIM_OCInitTypeDef *);
void TIM_OC4Init(TIM_TypeDef *, TIM_OCInitTypeDef *);
void TIM_OC4PreloadConfig(TIM_TypeDef *, uint16_t);
void TIM_ARRPreloadConfig(TIM_TypeDef *, FunctionalState);
void TIM_ITConfig(TIM_TypeDef *, uint16_t, FunctionalState);
void TIM_Cmd(TIM_TypeDef *, FunctionalState);
void TIM_InternalClockConfig(TIM_TypeDef *);
void TIM_CtrlPWMOutputs(TIM_TypeDef *, FunctionalState);
void TIM_SetCompare4(TIM_TypeDef *, uint16_t);
void TIM_ClearFlag(TIM_TypeDef *, uint16_t);
ITStatus TIM_GetITStatus(TIM_TypeDef *, uint16_t);
void TIM_ClearITPendingBit(TIM_TypeDef *, uint16_t);

/* DMA */
#define DMA_DIR_PeripheralDST 0x00000010u
#define DMA_DIR_PeripheralSRC 0x00000000u
#define DMA_PeripheralInc_Disable 0x00000000u
#define DMA_MemoryInc_Enable 0x00000080u
#define DMA_PeripheralDataSize_Byte 0x00000000u
#define DMA_PeripheralDataSize_HalfWord 0x00000100u
#define DMA_MemoryDataSize_Byte 0x00000000u
#define DMA_MemoryDataSize_HalfWord 0x00000400u
#define DMA_Mode_Circular 0x00000020u
#define DMA_Mode_Normal 0x00000000u
#define DMA_Priority_High 0x00002000u
#define DMA_M2M_Disable 0x00000000u
#define DMA_IT_TC 0x00000002u
#define DMA1_FLAG_TC4 0x00002000u
typedef struct {
    uint32_t DMA_PeripheralBaseAddr, DMA_MemoryBaseAddr, DMA_DIR;
    uint32_t DMA_BufferSize, DMA_PeripheralInc, DMA_MemoryInc;
    uint32_t DMA_PeripheralDataSize, DMA_MemoryDataSize;
    uint32_t DMA_Mode, DMA_Priority, DMA_M2M;
} DMA_InitTypeDef;
void DMA_DeInit(DMA_Channel_TypeDef *);
void DMA_Init(DMA_Channel_TypeDef *, DMA_InitTypeDef *);
void DMA_Cmd(DMA_Channel_TypeDef *, FunctionalState);
void DMA_ITConfig(DMA_Channel_TypeDef *, uint32_t, FunctionalState);
void DMA_ClearFlag(uint32_t);

/* ADC */
#define ADC_Mode_Independent 0x00000000u
#define ADC_ExternalTrigConv_None 0x000e0000u
#define ADC_DataAlign_Right 0x00000000u
#define ADC_SampleTime_239Cycles5 0x07u
#define ADC_IT_EOC 0x0220u
#define ADC_Channel_0 0u
#define ADC_Channel_1 1u
#define ADC_Channel_2 2u
#define ADC_Channel_3 3u
#define ADC_Channel_4 4u
#define ADC_Channel_5 5u
#define ADC_Channel_6 6u
#define ADC_Channel_7 7u
#define ADC_Channel_8 8u
#define ADC_Channel_9 9u
#define ADC_Channel_10 10u
#define ADC_Channel_11 11u
#define ADC_Channel_12 12u
#define ADC_Channel_13 13u
#define ADC_Channel_14 14u
#define ADC_Channel_15 15u
typedef struct {
    uint32_t ADC_Mode;
    FunctionalState ADC_ScanConvMode, ADC_ContinuousConvMode;
    uint32_t ADC_ExternalTrigConv, ADC_DataAlign;
    uint8_t ADC_NbrOfChannel;
} ADC_InitTypeDef;
void ADC_DeInit(ADC_TypeDef *);
void ADC_Init(ADC_TypeDef *, ADC_InitTypeDef *);
void ADC_RegularChannelConfig(ADC_TypeDef *, uint8_t, uint8_t, uint8_t);
void ADC_DMACmd(ADC_TypeDef *, FunctionalState);
void ADC_Cmd(ADC_TypeDef *, FunctionalState);
void ADC_ResetCalibration(ADC_TypeDef *);
FlagStatus ADC_GetResetCalibrationStatus(ADC_TypeDef *);
void ADC_StartCalibration(ADC_TypeDef *);
FlagStatus ADC_GetCalibrationStatus(ADC_TypeDef *);
void ADC_SoftwareStartConvCmd(ADC_TypeDef *, FunctionalState);
void ADC_ITConfig(ADC_TypeDef *, uint16_t, FunctionalState);
ITStatus ADC_GetITStatus(ADC_TypeDef *, uint16_t);
uint16_t ADC_GetConversionValue(ADC_TypeDef *);
void ADC_ClearITPendingBit(ADC_TypeDef *, uint16_t);

/* USART */
#define USART_WordLength_8b 0x0000u
#define USART_StopBits_1 0x0000u
#define USART_Parity_No 0x0000u
#define USART_Mode_Rx 0x0004u
#define USART_Mode_Tx 0x0008u
#define USART_HardwareFlowControl_None 0x0000u
#define USART_Clock_Disable 0x0000u
#define USART_CPOL_Low 0x0000u
#define USART_CPHA_2Edge 0x0200u
#define USART_LastBit_Disable 0x0000u
#define USART_IT_RXNE 0x0525u
#define USART_DMAReq_Tx 0x0080u
#define USART_FLAG_TC 0x0040u
#define USART_FLAG_RXNE 0x0020u
typedef struct {
    uint32_t USART_BaudRate;
    uint16_t USART_WordLength, USART_StopBits, USART_Parity;
    uint16_t USART_Mode, USART_HardwareFlowControl;
} USART_InitTypeDef;
typedef struct {
    uint16_t USART_Clock, USART_CPOL, USART_CPHA, USART_LastBit;
} USART_ClockInitTypeDef;
void USART_DeInit(USART_TypeDef *);
void USART_ClockInit(USART_TypeDef *, USART_ClockInitTypeDef *);
void USART_Init(USART_TypeDef *, USART_InitTypeDef *);
void USART_ClearFlag(USART_TypeDef *, uint16_t);
void USART_ITConfig(USART_TypeDef *, uint16_t, FunctionalState);
void USART_DMACmd(USART_TypeDef *, uint16_t, FunctionalState);
void USART_Cmd(USART_TypeDef *, FunctionalState);
ITStatus USART_GetITStatus(USART_TypeDef *, uint16_t);
uint16_t USART_ReceiveData(USART_TypeDef *);
void USART_ClearITPendingBit(USART_TypeDef *, uint16_t);

#ifndef assert_param
#define assert_param(expr) ((void)0)
#endif

#endif /* FX9A_LEGACY_PERIPHERAL_API_H */
