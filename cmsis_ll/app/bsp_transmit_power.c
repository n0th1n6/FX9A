#include "bsp_transmit_power.h"
#include "ll_pins.h"

//发射功率控制，分3档，大功率，中功率，小功率（在菜单里分3项：H/M/L   H/0-60  M/0-60  L/0-60) 
//PE14/45/CS   PE15/46/SDI   PB10/47/CLK

#define TP_CS   GPIO_BSRR_BS14
#define TP_SDI  GPIO_BSRR_BS15
#define TP_CLK  GPIO_BSRR_BS10
#define TP_CS_SDI_PORT GPIOE
#define TP_CLK_PORT GPIOB

// PE11/42脚  TX发射控制端（不管是SSB模式还是CW模式，在发射时输出高电平）
#define TX_CONTROL   GPIO_BSRR_BS11
// PB11/48脚    静音控制，在发射时输出高电平
// PB12/51脚    MIC控制，在CW模式下发射时输出低电平，RX时也输出低电平，只有在SSB模式，发射时输出高电平
#define Quiet GPIO_BSRR_BS11
#define MIC GPIO_BSRR_BS12

//PB13/52脚    CW滤波器切换控制，在SSB模式下默认低电平，在CW模式下 RX时默认高电平（N 窄带）。
//         在CM模式 发射时 为低电平（W 宽带）
//PB14/53脚    BFO切换 CW发射状态下，输出高电平; 其他情况全为低电平
#define CW_FIL_CONTROL GPIO_BSRR_BS13
#define CW_BFO_CONTROL GPIO_BSRR_BS14

void tp_init()
{
  LL_GPIO_InitTypeDef GPIO_InitStructure = {0};
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(TP_CS | TP_SDI | TX_CONTROL);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(TP_CS_SDI_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(TP_CLK | Quiet | MIC | CW_FIL_CONTROL | CW_BFO_CONTROL);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(TP_CLK_PORT, &GPIO_InitStructure);
  
}

void CW_FilterControl(uint8_t high)
{
  if( high )
  {
    (TP_CLK_PORT->BSRR = (CW_FIL_CONTROL));
  }
  else
  {
    (TP_CLK_PORT->BRR = (CW_FIL_CONTROL)); 
  }
}

void CW_BFOControl(uint8_t high)
{
  if( high )
  {
    (TP_CLK_PORT->BSRR = (CW_BFO_CONTROL));
  }
  else
  {
    (TP_CLK_PORT->BRR = (CW_BFO_CONTROL)); 
  }
}
void QuietModeOn()
{
  (TP_CLK_PORT->BSRR = (Quiet)); 
}

void QuietModeOff()
{
  (TP_CLK_PORT->BRR = (Quiet)); 
}

void MICControlOn()
{
  (TP_CLK_PORT->BSRR = (MIC)); 
}

void MICControlOff()
{
  (TP_CLK_PORT->BRR = (MIC)); 
}

void tx_control_on()
{
  (TP_CS_SDI_PORT->BSRR = (TX_CONTROL)); 
}

void tx_control_off()
{
  (TP_CS_SDI_PORT->BRR = (TX_CONTROL)); 
}

void tp_control(uint8_t data)
{
  (TP_CLK_PORT->BRR = (TP_CLK));  
  (TP_CLK_PORT->BSRR = (TP_CLK)); 
  
  (TP_CS_SDI_PORT->BRR = (TP_CS));

  for(uint8_t i=0; i<8; i++)
  {
    (TP_CLK_PORT->BRR = (TP_CLK)); 
    
    if(data & 0x80)
    {
      (TP_CS_SDI_PORT->BSRR = (TP_SDI));  
    }else
    {
      (TP_CS_SDI_PORT->BRR = (TP_SDI));  
    }
    data <<= 1;
    
   // delay_us(1);
    (TP_CLK_PORT->BSRR = (TP_CLK)); 
   // delay_us(1);
  }
  
  (TP_CS_SDI_PORT->BSRR = (TP_CS)); 
  (TP_CLK_PORT->BRR = (TP_CLK));  
  (TP_CLK_PORT->BSRR = (TP_CLK)); 
  (TP_CLK_PORT->BRR = (TP_CLK));  
}
