#include "bsp_receive_att.h"

// 接收衰减控制数字电位器  PD10/57/CLK   PD11/58/SDI    PD12/59/CS。 分3档 每档0-60
#define RATT_CS   GPIO_Pin_12
#define RATT_SDI  GPIO_Pin_11
#define RATT_CLK  GPIO_Pin_10
#define RATT_PORT GPIOD

void ratt_init()
{
  LL_GPIO_InitTypeDef GPIO_InitStructure = {0};
  
  GPIO_InitStructure.Pin = (((uint32_t)(RATT_CLK | RATT_SDI | RATT_CS) << GPIO_PIN_MASK_POS) | (RATT_CLK | RATT_SDI | RATT_CS));
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(RATT_PORT, &GPIO_InitStructure);
}

void ratt_control(uint8_t data)
{
  if(data > 60)
  {
    return;
  }
  (RATT_PORT->BRR = (RATT_CLK));  
  (RATT_PORT->BSRR = (RATT_CLK)); 
  
  (RATT_PORT->BRR = (RATT_CS));

  for(uint8_t i=0; i<8; i++)
  {
    (RATT_PORT->BRR = (RATT_CLK)); 
    
    if(data & 0x80)
    {
      (RATT_PORT->BSRR = (RATT_SDI));  
    }else
    {
      (RATT_PORT->BRR = (RATT_SDI));  
    }
    data <<= 1;
    
   // delay_us(1);
    (RATT_PORT->BSRR = (RATT_CLK)); 
   // delay_us(1);
  }
  
  (RATT_PORT->BSRR = (RATT_CS)); 
  (RATT_PORT->BRR = (RATT_CLK));  
  (RATT_PORT->BSRR = (RATT_CLK)); 
  (RATT_PORT->BRR = (RATT_CLK));  
}