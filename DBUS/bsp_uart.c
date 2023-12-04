/*
 * @Author: Yangyujian 2480383735@qq.com
 * @Date: 2023-11-22 09:37:02
 * @LastEditors: Yangyujian 2480383735@qq.com
 * @LastEditTime: 2023-12-01 17:32:28
 * @FilePath: \HTD-85H\DBUS\bsp_uart.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */

                                                                                                              
#include "string.h"
#include "stdlib.h"
#include "bsp_uart.h"
#include "usart.h"
#include "main.h"
uint8_t   dbus_buf[DBUS_BUFLEN];
rc_info_t rc;



/**
  * @brief      enable global uart it and do not use DMA transfer done it
  * @param[in]  huart: uart IRQHandler id
  * @param[in]  pData: receive buff 
  * @param[in]  Size:  buff size
  * @retval     set success or fail
  */
 /**
  * 
  * 
  * @param
 */
static int uart_receive_dma_no_it(UART_HandleTypeDef* huart, uint8_t* pData, uint32_t Size)
{
  uint32_t tmp1 = 0;

  tmp1 = huart->RxState;
	
	if (tmp1 == HAL_UART_STATE_READY)
	{
		if ((pData == NULL) || (Size == 0))
		{
			return HAL_ERROR;
		}

		huart->pRxBuffPtr = pData;
		huart->RxXferSize = Size;
		huart->ErrorCode  = HAL_UART_ERROR_NONE;

		/* Enable the DMA Stream */
		HAL_DMA_Start(huart->hdmarx, (uint32_t)&huart->Instance->DR, (uint32_t)pData, Size);

		/* 
		 * Enable the DMA transfer for the receiver request by setting the DMAR bit
		 * in the UART CR3 register 
		 */
		SET_BIT(huart->Instance->CR3, USART_CR3_DMAR);

		return HAL_OK;
	}
	else
	{
		return HAL_BUSY;
	}
}

/**
  * @brief      returns the number of remaining data units in the current DMAy Streamx transfer.
  * @param[in]  dma_stream: where y can be 1 or 2 to select the DMA and x can be 0
  *             to 7 to select the DMA Stream.
  * @retval     The number of remaining data units in the current DMAy Streamx transfer.
  */
uint16_t dma_current_data_counter(DMA_Stream_TypeDef *dma_stream)
{
  /* Return the number of remaining data units for DMAy Streamx */
  return ((uint16_t)(dma_stream->NDTR));
}



/**
  * @brief       handle received rc data
  * @param[out]  rc:   structure to save handled rc data
  * @param[in]   buff: the buff which saved raw rc data
  * @retval 
  */

void rc_callback_handler(rc_info_t *rc, uint8_t *buff)
{

	rc->rx_once=1;
	rc->header=buff[0];
	rc->ch1 = (buff[1]) | (((buff[2]&0x07)<<8));//ch1----原地转
	rc->ch1 -= 1024;//ch2----推杆升降
	rc->ch2 = (((buff[2]&0xf8)>>3))|(((buff[3]&0x3f))<<5) ;
	rc->ch2 -= 1024;
	rc->ch3 = (((buff[3]&0xc0)>>6))|(((buff[4])<<2))|((buff[5]&0x01)<<10);//ch3----前进后退 rc->ch3 -= 1024;
	rc->ch3 -= 1024;
	rc->ch4 = (((buff[5]&0xfe)>>1))|(((buff[6]&0x0f))<<7);//ch4----左右转向
	rc->ch4 -= 1024;
	rc->sw1 = (((buff[7]&0x80)>>7))|((buff[8])<<1)|(((buff[9]&0x03))<<9);//ch6----SWA----原点校准
	rc->ch6 = (((buff[7]&0x80)>>7))|((buff[8])<<1) | (((buff[9]&0x03))<9);
	rc->ch8 = ((buff[10] >>5)| (buff[11] << 3)) & 0x07FF;//ch8----SWB----遥控器上位机选择开关
	rc->sw2 = ((buff[10] >>5)| (buff[11] << 3)) & 0x07FF;
	rc->sw3 = ((buff[9]>>2)|((buff[10]<<6)&0x07ff));//ch7---SWC----驻车开关(由于fusi遥控器ch1,ch2,ch3,ch4是固定的不能重新配置)
	rc->ch7 = ((buff[9]>>2)|((buff[10]<<6)&0x07ff));
	rc->sw4 = (((buff[6]&0xf0)>>4))|(((buff[7]&0x7f))<<4);//ch5----SWD----电机上下电
	rc->ch5 = (((buff[6]&0xf0)>>4))|(((buff[7]&0x7f))<<4); 
	rc->flag=buff[23]; 
	rc->ender=buff[24];
}

/**
  * @brief      clear idle it flag after uart receive a frame data
  * @param[in]  huart: uart IRQHandler id
  * @retval  
  */
static void uart_rx_idle_callback(UART_HandleTypeDef* huart)
{
	/* clear idle it flag avoid idle interrupt all the time */
	__HAL_UART_CLEAR_IDLEFLAG(huart);

	/* handle received data in idle interrupt */
	if (huart == &DBUS_HUART)
	{
		/* clear DMA transfer complete flag */
		__HAL_DMA_DISABLE(huart->hdmarx);

		/* handle dbus data dbus_buf from DMA */
		if ((DBUS_MAX_LEN - dma_current_data_counter(huart->hdmarx->Instance)) == DBUS_BUFLEN)
		{
		//	rc_callback_handler(&rc, dbus_buf);	
		}
		rc_callback_handler(&rc, dbus_buf);	
		/* restart dma transmission */
		__HAL_DMA_SET_COUNTER(huart->hdmarx, DBUS_MAX_LEN);
		__HAL_DMA_ENABLE(huart->hdmarx);
	}
}

/**
  * @brief      callback this function when uart interrupt 
  * @param[in]  huart: uart IRQHandler id
  * @retval  
  */
void uart_receive_handler(UART_HandleTypeDef *huart)
{  
	if (__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE) && 
			__HAL_UART_GET_IT_SOURCE(huart, UART_IT_IDLE))
	{
		uart_rx_idle_callback(huart);
	}
	//uart_rx_idle_callback(huart);
}

/**
  * @brief   initialize dbus uart device 
  * @param   
  * @retval  
  */
void dbus_uart_init(void)
{
	/* open uart idle it */
	__HAL_UART_CLEAR_IDLEFLAG(&DBUS_HUART);
	__HAL_UART_ENABLE_IT(&DBUS_HUART, UART_IT_IDLE);

	uart_receive_dma_no_it(&DBUS_HUART, dbus_buf, DBUS_MAX_LEN);
}




