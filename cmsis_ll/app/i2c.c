#include "i2c.h"

//----- ST24C16 I2C configure -------


#define GET_I2C_SDA_PIN() 	( ((I2C_PORT->IDR & (I2C_SDA)) != 0u) )

#define I2C_DELAY_US 15

void SET_E2P_MODE(uint8_t bit)
{
  if( bit )
  {
    (I2C_PORT->BSRR = (E2P_WC));
  }
  else
  {
    (I2C_PORT->BRR = (E2P_WC));
  }
}
void SET_I2C_SCK_PIN(uint8_t bit)
{
  if( bit )
  {
    (I2C_PORT->BSRR = (I2C_SCK));
  }
  else
  {
    (I2C_PORT->BRR = (I2C_SCK));
  }
}

void SET_I2C_SDA_PIN(uint8_t bit)
{
  if( bit )
  {
    (I2C_PORT->BSRR = (I2C_SDA));
  }
  else
  {
    (I2C_PORT->BRR = (I2C_SDA));
  }
}

/* Match StdPeriph GPIO_Mode_Out_OD / GPIO_Mode_IN_FLOATING exactly.
 * STM32F1 CRL: each pin has CNF[1:0]:MODE[1:0] (four bits).
 * PE4 output open-drain at 2 MHz = 0x6; floating input = 0x4.
 * Only PE4's nibble is changed, leaving other GPIOE pins untouched.
 */
void SET_I2C_SDA_DIR(uint8_t out)
{
  const uint32_t shift = 4u * 4u; /* PE4 in GPIOE->CRL */
  const uint32_t mode = out ? 0x6u : 0x4u;
  I2C_PORT->CRL = (I2C_PORT->CRL & ~(0xfu << shift)) |
                  (mode << shift);
}

void i2c_reset()
{
  SET_E2P_MODE(0);
  SET_I2C_SDA_PIN(1);
  SET_I2C_SCK_PIN(0);
  delay_us(I2C_DELAY_US); // 钳住总线准备发数据
  
  for(uint8_t i=0; i<9; i++)
  {
    SET_I2C_SCK_PIN(1);
    delay_us(I2C_DELAY_US); // 钳住总线准备发数据
    SET_I2C_SCK_PIN(0);
    delay_us(I2C_DELAY_US); // 钳住总线准备发数据
  }
}

void i2c_init(void)
{
  /* Release both I2C lines and assert write protect before enabling
   * outputs. board_init() has already enabled the GPIOE clock.
   * The 24LC16 is not written by this initialization routine.
   */
  I2C_PORT->BSRR = E2P_WC | I2C_SDA | I2C_SCK;

  /* Exact STM32F1 CRL equivalents of the working StdPeriph setup:
   * PE2: GPIO_Mode_Out_PP, 2 MHz  -> 0x2
   * PE3: GPIO_Mode_Out_OD, 2 MHz  -> 0x6
   * PE4: GPIO_Mode_Out_OD, 2 MHz  -> 0x6
   */
  const uint32_t mask = (0xfu << 8) | (0xfu << 12) | (0xfu << 16);
  const uint32_t config = (0x2u << 8) | (0x6u << 12) | (0x6u << 16);
  I2C_PORT->CRL = (I2C_PORT->CRL & ~mask) | config;
}

void i2c_start()
{
  SET_I2C_SCK_PIN(1);
  SET_I2C_SDA_PIN(1);
  delay_us(I2C_DELAY_US); // 起始条件建立时间大于4.7us 
  
  SET_I2C_SDA_PIN(0);
  delay_us(I2C_DELAY_US); // 起始条件锁定时间大于4us 
  
  SET_I2C_SCK_PIN(0);
  delay_us(I2C_DELAY_US); // 钳住总线准备发数据
}

void i2c_stop()
{
  SET_I2C_SDA_PIN(0);
  SET_I2C_SCK_PIN(1);     // 发送总线时钟信号
  delay_us(I2C_DELAY_US); // 结束总线时间大于4us 
  
  SET_I2C_SDA_PIN(1);
  delay_us(I2C_DELAY_US); // 结束总线
}

void i2c_ack()
{
  SET_I2C_SDA_PIN(0);
  SET_I2C_SCK_PIN(1);
  delay_us(I2C_DELAY_US);
  
  SET_I2C_SCK_PIN(0);
  SET_I2C_SDA_PIN(1);
  delay_us(I2C_DELAY_US);
}

void i2c_noack()
{

  SET_I2C_SDA_PIN(1);
  SET_I2C_SCK_PIN(1);
  delay_us(I2C_DELAY_US);
  
  SET_I2C_SCK_PIN(0);
  SET_I2C_SDA_PIN(0);
  delay_us(I2C_DELAY_US);
}

uint8_t i2c_getack()
{
  uint8_t val = 0;
  
  /* 为输入做准备，确保从机ACK真正为低 */
  SET_I2C_SDA_PIN(1);
  SET_I2C_SDA_DIR(INPUT);
  SET_I2C_SCK_PIN(1);
  delay_us(I2C_DELAY_US);
  
  if( GET_I2C_SDA_PIN() )
    val = 1;
  SET_I2C_SCK_PIN(0);
  delay_us(I2C_DELAY_US);
  SET_I2C_SDA_DIR(OUTPUT);
  
  return ( (val==1)?NOACK:ACK );
}

void i2c_tx(uint8_t wdata)
{
  uint8_t i;

  for(i=0;i<8;i++)
  {
    if( (wdata & 0x80) == 0x80)
      SET_I2C_SDA_PIN(1);
    else
      SET_I2C_SDA_PIN(0);
    wdata <<= 1;
    SET_I2C_SCK_PIN(1);
    delay_us(I2C_DELAY_US);	//Confirm the time of csk high level over 4.7us
    SET_I2C_SCK_PIN(0);
    delay_us(I2C_DELAY_US);  
  }
  SET_I2C_SDA_PIN(1);
}

uint8_t i2c_rx()
{
  uint8_t i;
  uint8_t tmp = 0;
  

  SET_I2C_SDA_PIN(1);	//Release data bus
  SET_I2C_SDA_DIR(INPUT);
  for(i=0;i<8;i++)
  {
    
    SET_I2C_SCK_PIN(1);
    delay_us(I2C_DELAY_US);
    tmp <<= 1;
    if( GET_I2C_SDA_PIN() )
      tmp |= 0x01;
    SET_I2C_SCK_PIN(0);
    delay_us(I2C_DELAY_US);
  }
  SET_I2C_SDA_DIR(OUTPUT);
  return tmp;
}

