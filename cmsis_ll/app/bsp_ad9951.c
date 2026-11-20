#include "bsp_ad9951.h"
#include "ll_pins.h"

// DDS  AD9951¿ØÖÆ½Ó¿Ú  PD13/60/UPD  PD14/61/SDIO   PD15/62/CLK   PC6/63/RET
#define AD9951_UPD  GPIO_BSRR_BS13
#define AD9951_SDIO GPIO_BSRR_BS14
#define AD9951_CLK  GPIO_BSRR_BS15
#define AD9951_RET  GPIO_BSRR_BS6
#define AD9951_UPD_SDIO_CLK_PORT GPIOD
#define AD9951_RET_PORT GPIOC


#define IF 9000000

// AD9951 SCLK maximum frequency is 25 MHz

uint32_t dds_freq;
uint8_t RegisterData[4] = {0,0,0,0};

void ad9951_sdio_out()
{
  LL_GPIO_InitTypeDef GPIO_InitStructure = {0};
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(AD9951_SDIO);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(AD9951_UPD_SDIO_CLK_PORT, &GPIO_InitStructure);
}

void ad9951_sdio_in()
{
  LL_GPIO_InitTypeDef GPIO_InitStructure = {0};
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(AD9951_SDIO);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStructure.Pull = LL_GPIO_PULL_UP;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(AD9951_UPD_SDIO_CLK_PORT, &GPIO_InitStructure);
}

void ad9951_write(uint8_t RegisterAddress, uint8_t NumberofRegisters, uint8_t *RegisterData)
{
  uint8_t ControlValue = 0;
  uint8_t ValueToWrite = 0;
  int8_t RegisterIndex = 0;
  
  //Create the 8-bit header
  ControlValue = RegisterAddress;
        
  (AD9951_UPD_SDIO_CLK_PORT->BSRR = (AD9951_CLK));
  ad9951_sdio_out(); //Make SDIO an output
  
  //Write out the control word
  for(uint8_t i=0; i<8; i++)
  {
    (AD9951_UPD_SDIO_CLK_PORT->BRR = (AD9951_CLK));
    if(0x80 == (ControlValue & 0x80))
    {
      (AD9951_UPD_SDIO_CLK_PORT->BSRR = (AD9951_SDIO));	  //Send one to SDIO pin
    }
    else
    {
      (AD9951_UPD_SDIO_CLK_PORT->BRR = (AD9951_SDIO)); 	  //Send zero to SDIO pin
    }
    (AD9951_UPD_SDIO_CLK_PORT->BSRR = (AD9951_CLK)); 
    ControlValue <<= 1;	//Rotate data
  }
  //(AD9951_UPD_SDIO_CLK_PORT->BRR = (AD9951_CLK));

  //And then the data
  for (RegisterIndex=NumberofRegisters; RegisterIndex>0; RegisterIndex--)
  {
    ValueToWrite = *(RegisterData + RegisterIndex - 1);
    for (uint8_t i=0; i<8; i++)	
    {
      (AD9951_UPD_SDIO_CLK_PORT->BRR = (AD9951_CLK));
      if(0x80 == (ValueToWrite & 0x80))
      {
        (AD9951_UPD_SDIO_CLK_PORT->BSRR = (AD9951_SDIO));	  //Send one to SDIO pin
      }
      else
      {
        (AD9951_UPD_SDIO_CLK_PORT->BRR = (AD9951_SDIO));	  //Send zero to SDIO pin
      }

      (AD9951_UPD_SDIO_CLK_PORT->BSRR = (AD9951_CLK));
      ValueToWrite <<= 1;	//Rotate data
    }
  }
  //(AD9951_UPD_SDIO_CLK_PORT->BRR = (AD9951_CLK));
}

void ad9951_read(uint8_t RegisterAddress, uint8_t NumberofRegisters, uint8_t *RegisterData)
{
  uint8_t ControlValue = 0;
  int8_t RegisterIndex = 0;
  uint8_t ReceiveData = 0;
  uint8_t i = 0;
  
  //Create the 8-bit header
  ControlValue = RegisterAddress;
        
  (AD9951_UPD_SDIO_CLK_PORT->BSRR = (AD9951_CLK));
  ad9951_sdio_out();

  //Write out the control word
  for(i=0; i<8; i++)
  {
    (AD9951_UPD_SDIO_CLK_PORT->BRR = (AD9951_CLK));
    if(0x80 == (ControlValue & 0x80))
    {
      (AD9951_UPD_SDIO_CLK_PORT->BSRR = (AD9951_SDIO));	  //Send one to SDIO pin
    }
    else
    {
      (AD9951_UPD_SDIO_CLK_PORT->BRR = (AD9951_SDIO));	  //Send zero to SDIO pin
    }

    (AD9951_UPD_SDIO_CLK_PORT->BSRR = (AD9951_CLK));
    ControlValue <<= 1;	//Rotate data
  }
  delay_us(10);
  ad9951_sdio_in();	//Make SDA an input

  //Read data in
  for (RegisterIndex=NumberofRegisters; RegisterIndex>0; RegisterIndex--)
  {
    for(i=0; i<8; i++)
    {     
      (AD9951_UPD_SDIO_CLK_PORT->BSRR = (AD9951_CLK));
      delay_us(1);
      (AD9951_UPD_SDIO_CLK_PORT->BRR = (AD9951_CLK));
      ReceiveData <<= 1;		//Rotate data	
      if(((AD9951_UPD_SDIO_CLK_PORT->IDR & (AD9951_SDIO)) != 0u)) //Read SDIO of AD9959
      {
        ReceiveData |= 1;	
      }
      
    }
    *(RegisterData + RegisterIndex - 1) = ReceiveData;
  }

}

void ad9951_update_low()
{  
  (AD9951_UPD_SDIO_CLK_PORT->BRR = (AD9951_UPD));
}

void ad9951_update_high()
{  
  (AD9951_UPD_SDIO_CLK_PORT->BSRR = (AD9951_UPD)); 
}

void ad9951_update()
{ 
  (AD9951_UPD_SDIO_CLK_PORT->BRR = (AD9951_UPD));
  delay_us(2);
  (AD9951_UPD_SDIO_CLK_PORT->BSRR = (AD9951_UPD)); 
  delay_us(8);
  (AD9951_UPD_SDIO_CLK_PORT->BRR = (AD9951_UPD));
}


void ad9951_write_freq(uint32_t freq, uint32_t ref)
{
  uint32_t ftw;
  ftw = (freq * 4294967296) / ref;
//  if( ftw > 0x7fffffff)
//  {
//    ftw = 4294967296 - ftw;
//  }
  ad9951_write(FTW0_REG_ADDR, 4, (uint8_t *)&ftw);

  ad9951_update();
}

void ad9951_init()
{
  LL_GPIO_InitTypeDef GPIO_InitStructure = {0};
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(AD9951_UPD | AD9951_SDIO | AD9951_CLK);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(AD9951_UPD_SDIO_CLK_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(AD9951_RET);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(AD9951_RET_PORT, &GPIO_InitStructure);
  
  (AD9951_UPD_SDIO_CLK_PORT->BSRR = (AD9951_CLK));
  (AD9951_RET_PORT->BSRR = (AD9951_RET));
  (AD9951_RET_PORT->BRR = (AD9951_RET));
  ad9951_update_low();
   
  delay_ms(1);
  
  uint32_t tmp = 0x000023;
//  RegisterData[2] = 0x23;
//  RegisterData[1] = 0x00;
//  RegisterData[0] = 0x00;
 // ad9951_read(CFR2_REG_ADDR | 0x80, 3, (uint8_t *)&dds_freq);
  ad9951_write(CFR2_REG_ADDR, 3, (uint8_t *)&tmp);
//  ad9951_write_freq(20050000, 200000000);
//  ad9951_read(CFR1_REG_ADDR | 0x80, 4, (uint8_t *)&dds_freq);
//  ad9951_read(CFR2_REG_ADDR | 0x80, 3, (uint8_t *)&dds_freq);
  //dds_freq = 0;
  //ad9951_read(FTW0_REG_ADDR | 0x80, 4, (uint8_t *)&dds_freq);

}

uint32_t ad9951_freq_old = 0;
static uint32_t clk_ref_old = 0;
void lo_update(uint32_t freq, uint32_t ref)
{
  if( ad9951_freq_old != freq || clk_ref_old != ref)
  {
    ad9951_write_freq(IF+freq, ref*4);
    ad9951_freq_old = freq;
    clk_ref_old = ref;
  }
}