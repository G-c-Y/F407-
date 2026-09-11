#ifndef DVC_LCD_TOUCH_HQ
#define DVC_LCD_TOUCH_HQ

#ifdef __cplusplus
extern "C" {
#endif
//ֻ����2.8�������

#ifndef __DVC_LCD_TOUCH_H__
#define __DVC_LCD_TOUCH_H__

#include "stm32f4xx_hal.h"
#include "dvc_lcd.h"



 /* �������Ŷ�д�꣨CubeMX ��������ţ�����ֻ�����д�� */
  #define T_CS_SET     HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET)
  #define T_CS_CLR     HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET)
  #define T_SCK_SET    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,  GPIO_PIN_SET)
  #define T_SCK_CLR    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,  GPIO_PIN_RESET)
  #define T_MOSI_SET   HAL_GPIO_WritePin(GPIOF, GPIO_PIN_11, GPIO_PIN_SET)
  #define T_MOSI_CLR   HAL_GPIO_WritePin(GPIOF, GPIO_PIN_11, GPIO_PIN_RESET)
  #define T_MISO_GET   HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2)
  #define T_PEN_GET    HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1)

  #define LCD_W   240
  #define LCD_H   320

  void    TP_Init(void);
  uint8_t TP_Get_Calibrated(uint16_t *x, uint16_t *y);

#endif


#ifdef __cplusplus
}
#endif
#endif

