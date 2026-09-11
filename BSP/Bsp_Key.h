#ifndef BSP_KEY_H
#define BSP_KEY_H

#include "stm32f4xx_hal.h"
#include "main.h"
//防止bool报错
#include <stdbool.h>

/*按键宏定义*/
#define KEY_PORT GPIOE
#define KEY_PIN0 GPIO_PIN_4
#define KEY_PIN1 GPIO_PIN_3
#define KEY_PIN2 GPIO_PIN_2

extern bool key0Pressed;
extern bool key1Pressed;    
extern bool key2Pressed;

#endif /* BSP_KEY_H */

