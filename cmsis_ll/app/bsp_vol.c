#include "bsp_vol.h"
#include "ll_pins.h"

//数字音量电位器控制接口 PA6/31/CLK  PA7/32/SDI  PC4/33/CS
#define VOL_CS   GPIO_BSRR_BS4
#define VOL_SDI  GPIO_BSRR_BS7
#define VOL_CLK  GPIO_BSRR_BS6
#define VOL_CLK_SDI_PORT GPIOA
#define VOL_CS_PORT GPIOC

void vol_init()
{
  LL_GPIO_InitTypeDef GPIO_InitStructure = {0};
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(VOL_CLK | VOL_SDI);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(VOL_CLK_SDI_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(VOL_CS);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(VOL_CS_PORT, &GPIO_InitStructure);
}

void vol_control(uint8_t data)
{
  uint8_t wdat = 0;
  if(data > 60)
  {
    return;
  }
  
  wdat = data * 4;
  if (data == 60) {
    wdat = 255;
  }
  
  (VOL_CLK_SDI_PORT->BRR = (VOL_CLK));  
  (VOL_CLK_SDI_PORT->BSRR = (VOL_CLK)); 
  
  (VOL_CS_PORT->BRR = (VOL_CS));

  for(uint8_t i=0; i<8; i++)
  {
    (VOL_CLK_SDI_PORT->BRR = (VOL_CLK)); 
    
    if(wdat & 0x80)
    {
      (VOL_CLK_SDI_PORT->BSRR = (VOL_SDI));  
    }else
    {
      (VOL_CLK_SDI_PORT->BRR = (VOL_SDI));  
    }
    wdat <<= 1;
    
    delay_us(1);
    (VOL_CLK_SDI_PORT->BSRR = (VOL_CLK)); 
    delay_us(1);
  }
  
  (VOL_CS_PORT->BSRR = (VOL_CS)); 
  (VOL_CLK_SDI_PORT->BRR = (VOL_CLK));  
  (VOL_CLK_SDI_PORT->BSRR = (VOL_CLK)); 
  (VOL_CLK_SDI_PORT->BRR = (VOL_CLK));  
}