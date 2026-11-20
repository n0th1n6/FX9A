#include "bsp_usart.h"
#include "bsp_kenwood.h"
#include "stm32f1xx_ll_bus.h"
#include "stm32f1xx_ll_dma.h"
#include "stm32f1xx_ll_usart.h"
#include "stm32f1xx_ll_gpio.h"

#define IC_706 0x48
#define CIV_ADDR IC_706

unsigned char UartRxLen = 0;
unsigned char UartRxBuf[32];
static bool UartRxDiscard = false;
volatile uint32_t UartRxOverflowCount = 0;

static UartTxBufDef UTBuffer;
static bool DMA_IsReady = true;

static uint8_t Uart_Send_Buffer[64];

//uint8_t viewBuf[64];
//uint8_t viewLen = 0;

void UartBufInit()
{
  UTBuffer.head = 0;
  UTBuffer.rear = 0;
  UTBuffer.size	= 0;
  memset(UTBuffer.dat, 0, sizeof UTBuffer.dat);
}

void dma_init(void)
{
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
  NVIC_SetPriorityGrouping(5u);
  NVIC_SetPriority(DMA1_Channel4_IRQn, NVIC_EncodePriority(5u, 3u, 2u));
  NVIC_EnableIRQ(DMA1_Channel4_IRQn);

  LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_4);
  LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_4,
                          (uint32_t)(uintptr_t)&USART1->DR);
  LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_4,
                          (uint32_t)(uintptr_t)Uart_Send_Buffer);
  LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_4, sizeof Uart_Send_Buffer);
  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_4, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_4, LL_DMA_PERIPH_NOINCREMENT);
  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_4, LL_DMA_MEMORY_INCREMENT);
  LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_4, LL_DMA_PDATAALIGN_BYTE);
  LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_4, LL_DMA_MDATAALIGN_BYTE);
  LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_4, LL_DMA_MODE_NORMAL);
  LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_4, LL_DMA_PRIORITY_HIGH);
  LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_4);
}
void usart_init(void)
{
  UartBufInit();
  dma_init();
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1 |
                           LL_APB2_GRP1_PERIPH_GPIOA |
                           LL_APB2_GRP1_PERIPH_AFIO);
  LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_USART1);
  LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_USART1);

  /* PA9 USART1 TX alternate PP 50 MHz; PA10 floating RX input. */
  GPIOA->CRH = (GPIOA->CRH & ~((0xfu << 4) | (0xfu << 8)))
             | (0xbu << 4) | (0x4u << 8);

  USART1->CR1 = USART_CR1_TE | USART_CR1_RE;
  USART1->CR2 = 0u;
  USART1->CR3 = USART_CR3_DMAT;
  LL_USART_SetBaudRate(USART1, 72000000u,
                       LL_USART_OVERSAMPLING_16, 115200u);
  NVIC_SetPriorityGrouping(5u);
  NVIC_SetPriority(USART1_IRQn, NVIC_EncodePriority(5u, 1u, 1u));
  NVIC_EnableIRQ(USART1_IRQn);
  USART1->CR1 |= USART_CR1_RXNEIE;
  LL_USART_Enable(USART1);
}

void UartTxBufWrite(unsigned char wdat)
{
  if(UTBuffer.size < UART_TX_BUF_MAX_SIZE)
  {
    UTBuffer.dat[ UTBuffer.rear++ ] = wdat;
    if(UTBuffer.rear >= UART_TX_BUF_MAX_SIZE)
    {
      UTBuffer.rear = 0;
    }
    UTBuffer.size ++;	
  }
}

void UartTx(uint8_t *wdat, uint8_t len)
{
  for(uint8_t i=0; i<len; i++)
  {
    UartTxBufWrite(wdat[i]);  
  }
}

unsigned char UartTxBufRead()
{
  unsigned char rval = 0;
  if(UTBuffer.size > 0)
  {
    rval = UTBuffer.dat[ UTBuffer.head++ ];
    if(UTBuffer.head >= UART_TX_BUF_MAX_SIZE)
    {
      UTBuffer.head = 0;
    }							
    UTBuffer.size --; 
  }
  
  return rval;
}

unsigned char UART_TxBufSize()
{
  return UTBuffer.size;
}

void usart_rx_analysis(uint8_t wdat)
{
  /* The CAT protocol is semicolon framed. Once a command is too long,
   * ignore all bytes until its delimiter; otherwise the suffix might
   * accidentally be interpreted as a fresh transmit command.
   */
  if (wdat == ';')
  {
    if (!UartRxDiscard && UartRxLen >= 2)
      Kenwood_CmdParser(UartRxBuf, UartRxLen);
    UartRxLen = 0;
    UartRxDiscard = false;
  }
  else if (!UartRxDiscard)
  {
    if (UartRxLen < sizeof UartRxBuf)
      UartRxBuf[UartRxLen++] = wdat;
    else
    {
      UartRxDiscard = true;
      UartRxLen = 0;
      ++UartRxOverflowCount;
    }
  }
}

bool DMA_IsTxComplete()
{
  return DMA_IsReady;
}

void CopyTBufToDMA()
{
  uint8_t len = UTBuffer.size;
  for(uint8_t i=0; i<len; i++)
  {
    Uart_Send_Buffer[i] = UartTxBufRead();
  }
}

void UART_DMAStart(uint8_t length)
{
  DMA_IsReady = 0;
  LL_DMA_ClearFlag_TC4(DMA1);      //清DMA发送完成标志
  LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_4);   //停止DMA
  
  //设置传输数据长度  
  LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_4, length);      //重设传输长度
  //打开DMA  
  LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_4);  
}

void USART1_IRQHandler( void )
{
  uint8_t dat;
  
  if (LL_USART_IsActiveFlag_RXNE(USART1))
  {   
    dat = LL_USART_ReceiveData8(USART1);
    //USART_ClearITPendingBit(USART1, USART_IT_RXNE);

    usart_rx_analysis(dat);  
  }
  

}

  //串口1DMA方式发送中断  
void DMA1_Channel4_IRQHandler(void)  
{  
  //清除标志位  
  LL_DMA_ClearFlag_TC4(DMA1);  
  
  //关闭DMA  
  LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_4);  

  //允许再次发送  
  DMA_IsReady = 1;  
} 