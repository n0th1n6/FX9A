#include "filter_map.h"
#include "bsp_baud.h"

//NE5090²¨¶Î¿ØÖÆ¶Ë£¬PB15/54/A0  PD8/55/A1  PD9/56/A2
#define A0 GPIO_Pin_15
#define A1 GPIO_Pin_8
#define A2 GPIO_Pin_9
#define NE5090_A0_PORT GPIOB
#define NE5090_A12_PORT GPIOD

//HC4028²¨¶Î¿ØÖÆ¶Ë£¬PC7/64-10(A)  PC8/65-13(B)   PC9/66-12(C)   PA8/67-11(D)
#define A GPIO_Pin_7
#define B GPIO_Pin_8
#define C GPIO_Pin_9
#define D GPIO_Pin_8
#define HC4028_ABC_PORT GPIOC
#define HC4028_D_PORT GPIOA

void baud_init()
{
  GPIO_InitTypeDef GPIO_InitStructure;
  
  GPIO_InitStructure.GPIO_Pin =  A0;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(NE5090_A0_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.GPIO_Pin =  A1 | A2;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(NE5090_A12_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.GPIO_Pin =  A | B | C;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(HC4028_ABC_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.GPIO_Pin =  D;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(HC4028_D_PORT, &GPIO_InitStructure);
}

void baud_chn_ctrl(uint8_t numOfbaud)
{
  Fx9aBandFilter mapping = fx9a_filter_for_band(numOfbaud);
  const uint8_t ne = mapping.ne5090_code;
  const uint8_t hc = mapping.hc4028_code;

  if (ne & 1u) NE5090_A0_PORT->BSRR = A0;
  else NE5090_A0_PORT->BRR = A0;
  NE5090_A12_PORT->BSRR = ((ne & 2u) ? A1 : 0u) |
                          ((ne & 4u) ? A2 : 0u);
  NE5090_A12_PORT->BRR = ((ne & 2u) ? 0u : A1) |
                         ((ne & 4u) ? 0u : A2);
  HC4028_ABC_PORT->BSRR = ((hc & 1u) ? A : 0u) |
                          ((hc & 2u) ? B : 0u) |
                          ((hc & 4u) ? C : 0u);
  HC4028_ABC_PORT->BRR = ((hc & 1u) ? 0u : A) |
                         ((hc & 2u) ? 0u : B) |
                         ((hc & 4u) ? 0u : C);
  if (hc & 8u) HC4028_D_PORT->BSRR = D;
  else HC4028_D_PORT->BRR = D;
}