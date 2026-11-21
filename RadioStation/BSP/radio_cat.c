#include "radio_cat.h"
#include "bsp_usart.h"

void RadioCAT_Task(void)
{
  usart_process_rx_commands();
  uint8_t size = 0;
  if (DMA_IsTxComplete())
  {
    size = UART_TxBufSize();
    if(size > 0){
      // 串口发送处理
      CopyTBufToDMA();
      UART_DMAStart( size );
    }
  }
}
