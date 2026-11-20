#include "bsp_oled.h"
#include "ll_pins.h"


//ÏÔÊ¾Æ÷¿ØÖÆ½Ó¿Ú  PB7/93/DSIN  PB8/95/SCLK  PB9/96/D/C  PE0/97/RES  PE1/98/CS 
#define OLED_DSIN GPIO_BSRR_BS7
#define OLED_SCLK GPIO_BSRR_BS8
#define OLED_DC GPIO_BSRR_BS9
#define OLED_RES GPIO_BSRR_BS0
#define OLED_CS GPIO_BSRR_BS1

#define OLED_DSIN_SCLK_DC_PORT GPIOB
#define OLED_RES_CS_PORT GPIOE

void spi_io_init()
{
  LL_GPIO_InitTypeDef GPIO_InitStructure = {0};
  GPIO_InitStructure.Pin = fx9a_ll_pins(OLED_DSIN);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(OLED_DSIN_SCLK_DC_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(OLED_SCLK);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(OLED_DSIN_SCLK_DC_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(OLED_DC);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(OLED_DSIN_SCLK_DC_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(OLED_RES);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(OLED_RES_CS_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(OLED_CS);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(OLED_RES_CS_PORT, &GPIO_InitStructure);
  
  
  (OLED_RES_CS_PORT->BRR = (OLED_RES));
  for(uint8_t i=0;i<200;i++)
  {
    delay_us(200);
  }
  (OLED_RES_CS_PORT->BSRR = (OLED_RES)); 
}

void spi_write_command(uint8_t cmd)
{
  (OLED_RES_CS_PORT->BRR = (OLED_CS));
  (OLED_DSIN_SCLK_DC_PORT->BRR = (OLED_DC));
  for(uint8_t i=0; i<8; i++)
  {
    (OLED_DSIN_SCLK_DC_PORT->BRR = (OLED_SCLK)); 
    
    if(cmd & 0x80)
    {
      (OLED_DSIN_SCLK_DC_PORT->BSRR = (OLED_DSIN));  
    }else
    {
      (OLED_DSIN_SCLK_DC_PORT->BRR = (OLED_DSIN));  
    }
    cmd <<= 1;
    //delay_us(1);
    (OLED_DSIN_SCLK_DC_PORT->BSRR = (OLED_SCLK)); 
    //delay_us(1);
  }
  
  (OLED_DSIN_SCLK_DC_PORT->BSRR = (OLED_DC)); 
  (OLED_RES_CS_PORT->BSRR = (OLED_CS)); 
}

void spi_write_data(uint8_t data)
{
  (OLED_RES_CS_PORT->BRR = (OLED_CS));
  (OLED_DSIN_SCLK_DC_PORT->BSRR = (OLED_DC));
  for(uint8_t i=0; i<8; i++)
  {
    (OLED_DSIN_SCLK_DC_PORT->BRR = (OLED_SCLK)); 
    
    if(data & 0x80)
    {
      (OLED_DSIN_SCLK_DC_PORT->BSRR = (OLED_DSIN));  
    }else
    {
      (OLED_DSIN_SCLK_DC_PORT->BRR = (OLED_DSIN));  
    }
    data <<= 1;
    
   // delay_us(1);
    (OLED_DSIN_SCLK_DC_PORT->BSRR = (OLED_SCLK)); 
   // delay_us(1);
  }
  
  (OLED_DSIN_SCLK_DC_PORT->BSRR = (OLED_DC)); 
  (OLED_RES_CS_PORT->BSRR = (OLED_CS)); 
}

void Set_Column_Address(unsigned char a, unsigned char b)
{
  spi_write_command(0x15);			// Set Column Address
  spi_write_data(a);				//   Default => 0x00
  spi_write_data(b);				//   Default => 0x77
}

void Set_Row_Address(unsigned char a, unsigned char b)
{
  spi_write_command(0x75);			// Set Row Address
  spi_write_data(a);				//   Default => 0x00
  spi_write_data(b);				//   Default => 0x7F
}

void Set_Write_RAM()
{
  spi_write_command(0x5C);			// Enable MCU to Write into RAM
}


void Set_Read_RAM()
{
  spi_write_command(0x5D);			// Enable MCU to Read from RAM
}


void Set_Remap_Format(unsigned char d)
{
  spi_write_command(0xA0);			// Set Re-Map / Dual COM Line Mode
  spi_write_data(d);				//   Default => 0x40
                                                //     Horizontal Address Increment
                                                //     Column Address 0 Mapped to SEG0
                                                //     Disable Nibble Remap
                                                //     Scan from COM0 to COM[N-1]
                                                //     Disable COM Split Odd Even
  spi_write_data(0x11);			        //   Default => 0x01 (Disable Dual COM Mode)
}


void Set_Start_Line(unsigned char d)
{
  spi_write_command(0xA1);			// Set Vertical Scroll by RAM
  spi_write_data(d);				//   Default => 0x00
}


void Set_Display_Offset(unsigned char d)
{
  spi_write_command(0xA2);			// Set Vertical Scroll by Row
  spi_write_data(d);				//   Default => 0x00
}


void Set_Display_Mode(unsigned char d)
{
  spi_write_command(0xA4|d);			// Set Display Mode
  //   Default => 0xA4
  //     0xA4 (0x00) => Entire Display Off, All Pixels Turn Off
  //     0xA5 (0x01) => Entire Display On, All Pixels Turn On at GS Level 15
  //     0xA6 (0x02) => Normal Display
  //     0xA7 (0x03) => Inverse Display
}


void Set_Partial_Display(unsigned char a, unsigned char b, unsigned char c)
{
  spi_write_command(0xA8|a);
  // Default => 0x8F
  //   Select Internal Booster at Display On
  if(a == 0x00)
  {
    spi_write_data(b);
    spi_write_data(c);
  }
}


void Set_Function_Selection(unsigned char d)
{
  spi_write_command(0xAB);			// Function Selection
  spi_write_data(d);				//   Default => 0x01
  //     Enable Internal VDD Regulator
}


void Set_Display_On_Off(unsigned char d)
{
  spi_write_command(0xAE|d);			// Set Display On/Off
  //   Default => 0xAE
  //     0xAE (0x00) => Display Off (Sleep Mode On)
  //     0xAF (0x01) => Display On (Sleep Mode Off)
}


void Set_Phase_Length(unsigned char d)
{
  spi_write_command(0xB1);			// Phase 1 (Reset) & Phase 2 (Pre-Charge) Period Adjustment
  spi_write_data(d);				//   Default => 0x74 (7 Display Clocks [Phase 2] / 9 Display Clocks [Phase 1])
  //     D[3:0] => Phase 1 Period in 5~31 Display Clocks
  //     D[7:4] => Phase 2 Period in 3~15 Display Clocks
}


void Set_Display_Clock(unsigned char d)
{
  spi_write_command(0xB3);			// Set Display Clock Divider / Oscillator Frequency
  spi_write_data(d);				//   Default => 0xD0
  //     A[3:0] => Display Clock Divider
  //     A[7:4] => Oscillator Frequency
}


void Set_Display_Enhancement_A(unsigned char a, unsigned char b)
{
  spi_write_command(0xB4);			// Display Enhancement
  spi_write_data(0xA0|a);			//   Default => 0xA2
  //     0xA0 (0x00) => Enable External VSL
  //     0xA2 (0x02) => Enable Internal VSL (Kept VSL Pin N.C.)
  spi_write_data(0x05|b);			//   Default => 0xB5
  //     0xB5 (0xB0) => Normal
  //     0xFD (0xF8) => Enhance Low Gray Scale Display Quality
}


void Set_GPIO(unsigned char d)
{
  spi_write_command(0xB5);			// General Purpose IO
  spi_write_data(d);				//   Default => 0x0A (GPIO Pins output Low Level.)
}


void Set_Precharge_Period(unsigned char d)
{
  spi_write_command(0xB6);			// Set Second Pre-Charge Period
  spi_write_data(d);				//   Default => 0x08 (8 Display Clocks)
}


void Set_Precharge_Voltage(unsigned char d)
{
  spi_write_command(0xBB);			// Set Pre-Charge Voltage Level
  spi_write_data(d);				//   Default => 0x17 (0.50*VCC)
}


void Set_VCOMH(unsigned char d)
{
  spi_write_command(0xBE);			// Set COM Deselect Voltage Level
  spi_write_data(d);				//   Default => 0x04 (0.80*VCC)
}


void Set_Contrast_Current(unsigned char d)
{
  spi_write_command(0xC1);			// Set Contrast Current
  spi_write_data(d);				//   Default => 0x7F
}


void Set_Master_Current(unsigned char d)
{
  spi_write_command(0xC7);			// Master Contrast Current Control
  spi_write_data(d);				//   Default => 0x0f (Maximum)
}


void Set_Multiplex_Ratio(unsigned char d)
{
  spi_write_command(0xCA);			// Set Multiplex Ratio
  spi_write_data(d);				//   Default => 0x7F (1/128 Duty)
}


void Set_Display_Enhancement_B(unsigned char d)
{
  spi_write_command(0xD1);			// Display Enhancement
  spi_write_data(0x82|d);			//   Default => 0xA2
  //     0x82 (0x00) => Reserved
  //     0xA2 (0x20) => Normal
  spi_write_data(0x20);
}


void Set_Command_Lock(unsigned char d)
{
  spi_write_command(0xFD);			// Set Command Lock
  spi_write_data(0x12|d);			//   Default => 0x12
  //     0x12 => Driver IC interface is unlocked from entering command.
  //     0x16 => All Commands are locked except 0xFD.
}
