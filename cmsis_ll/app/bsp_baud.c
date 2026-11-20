#include "bsp_baud.h"
#include "ll_pins.h"

//NE5090波段控制端，PB15/54/A0  PD8/55/A1  PD9/56/A2
#define A0 GPIO_Pin_15
#define A1 GPIO_Pin_8
#define A2 GPIO_Pin_9
#define NE5090_A0_PORT GPIOB
#define NE5090_A12_PORT GPIOD

//HC4028波段控制端，PC7/64-10(A)  PC8/65-13(B)   PC9/66-12(C)   PA8/67-11(D)
#define A GPIO_Pin_7
#define B GPIO_Pin_8
#define C GPIO_Pin_9
#define D GPIO_Pin_8
#define HC4028_ABC_PORT GPIOC
#define HC4028_D_PORT GPIOA

void baud_init()
{
  LL_GPIO_InitTypeDef GPIO_InitStructure = {0};
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(A0);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(NE5090_A0_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(A1 | A2);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(NE5090_A12_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(A | B | C);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(HC4028_ABC_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(D);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(HC4028_D_PORT, &GPIO_InitStructure);
}

void baud_chn_ctrl(uint8_t numOfbaud)
{
  /*
  *      原版   EN5090引脚修改：160M 第4脚/Q0     80M第5脚/Q1    60M/40M第6脚/Q2    30M第9脚/Q4   20M第7脚/Q3  17M/15M第11脚/Q6   12M/10M第10脚/Q5
  * 2016.03.09  EN5090引脚修改：160M 第11脚/Q6    80M第10脚/Q5   60M/40M第9脚/Q4    30M第7脚/Q3   20M第6脚/Q2  17M/15M第5脚/Q1    12M/10M第4脚/Q0
  */
  switch(numOfbaud)
  {
  case 0: // 160M   EN5090:Q6-输出低电平    HC4028:Y0-输出高电平
    (NE5090_A12_PORT->BSRR = (A2));
    (NE5090_A12_PORT->BSRR = (A1));
    (NE5090_A0_PORT->BRR = (A0));
    
    (HC4028_D_PORT->BRR = (D));
    (HC4028_ABC_PORT->BRR = (C));
    (HC4028_ABC_PORT->BRR = (B)); 
    (HC4028_ABC_PORT->BRR = (A)); 
    break;
  case 1: // 80M      EN5090:Q5-输出低电平    HC4028:Y1-输出高电平
    (NE5090_A12_PORT->BSRR = (A2));
    (NE5090_A12_PORT->BRR = (A1));
    (NE5090_A0_PORT->BSRR = (A0));
    
    (HC4028_D_PORT->BRR = (D));
    (HC4028_ABC_PORT->BRR = (C));
    (HC4028_ABC_PORT->BRR = (B)); 
    (HC4028_ABC_PORT->BSRR = (A));
    break;
  case 2: // 60M      EN5090:Q4-输出低电平    HC4028:Y8-输出高电平
    (NE5090_A12_PORT->BSRR = (A2));
    (NE5090_A12_PORT->BRR = (A1));
    (NE5090_A0_PORT->BRR = (A0));
     
    (HC4028_D_PORT->BSRR = (D));
    (HC4028_ABC_PORT->BRR = (C));
    (HC4028_ABC_PORT->BRR = (B)); 
    (HC4028_ABC_PORT->BRR = (A));
    break;
  case 3: // 40M      EN5090:Q4-输出低电平    HC4028:Y2-输出高电平
    (NE5090_A12_PORT->BSRR = (A2));
    (NE5090_A12_PORT->BRR = (A1));
    (NE5090_A0_PORT->BRR = (A0));
    
    (HC4028_D_PORT->BRR = (D));
    (HC4028_ABC_PORT->BRR = (C));
    (HC4028_ABC_PORT->BSRR = (B)); 
    (HC4028_ABC_PORT->BRR = (A)); 
    break;
  case 4: // 30M    EN5090:Q3-输出低电平    HC4028:Y3-输出高电平
    (NE5090_A12_PORT->BRR = (A2));
    (NE5090_A12_PORT->BSRR = (A1));
    (NE5090_A0_PORT->BSRR = (A0));
    
    (HC4028_D_PORT->BRR = (D));
    (HC4028_ABC_PORT->BRR = (C));
    (HC4028_ABC_PORT->BSRR = (B)); 
    (HC4028_ABC_PORT->BSRR = (A));
    break;
  case 5: // 20M      EN5090:Q2-输出低电平    HC4028:Y4-输出高电平
    (NE5090_A12_PORT->BRR = (A2));
    (NE5090_A12_PORT->BSRR = (A1));
    (NE5090_A0_PORT->BRR = (A0));
    
    (HC4028_D_PORT->BRR = (D));
    (HC4028_ABC_PORT->BSRR = (C));
    (HC4028_ABC_PORT->BRR = (B)); 
    (HC4028_ABC_PORT->BRR = (A));
    break;
  case 6: // 17M    EN5090:Q1-输出低电平    HC4028:Y5-输出高电平
    (NE5090_A12_PORT->BRR = (A2));
    (NE5090_A12_PORT->BRR = (A1));
    (NE5090_A0_PORT->BSRR = (A0)); 
    
    (HC4028_D_PORT->BRR = (D));
    (HC4028_ABC_PORT->BSRR = (C));
    (HC4028_ABC_PORT->BRR = (B)); 
    (HC4028_ABC_PORT->BSRR = (A));
    break;
  case 7: // 15M    EN5090:Q1-输出低电平    HC4028:Y6-输出高电平
    (NE5090_A12_PORT->BRR = (A2));
    (NE5090_A12_PORT->BRR = (A1));
    (NE5090_A0_PORT->BSRR = (A0)); 
    
    (HC4028_D_PORT->BRR = (D));
    (HC4028_ABC_PORT->BSRR = (C));
    (HC4028_ABC_PORT->BSRR = (B)); 
    (HC4028_ABC_PORT->BRR = (A));
    break;
  case 8: // 12M      EN5090:Q0-输出低电平    HC4028:Y7-输出高电平
    (NE5090_A12_PORT->BRR = (A2));
    (NE5090_A12_PORT->BRR = (A1));
    (NE5090_A0_PORT->BRR = (A0));
    
    (HC4028_D_PORT->BRR = (D));
    (HC4028_ABC_PORT->BSRR = (C));
    (HC4028_ABC_PORT->BSRR = (B)); 
    (HC4028_ABC_PORT->BSRR = (A));
    break;
  case 9: // 10M      EN5090:Q0-输出低电平    HC4028:Y8-输出高电平
    (NE5090_A12_PORT->BRR = (A2));
    (NE5090_A12_PORT->BRR = (A1));
    (NE5090_A0_PORT->BRR = (A0));
    
    (HC4028_D_PORT->BSRR = (D));
    (HC4028_ABC_PORT->BRR = (C));
    (HC4028_ABC_PORT->BRR = (B)); 
    (HC4028_ABC_PORT->BSRR = (A));
    break;
  }
}