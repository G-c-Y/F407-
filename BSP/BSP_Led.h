#ifndef BSP_LED_H
#define BSP_LED_H

#include "stm32f4xx_hal.h"
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LED_PORT GPIOF
#define GREEN_LED_PIN GPIO_PIN_10
#define RED_LED_PIN GPIO_PIN_9

void GreenLed_ON(void);
void RedLed_ON(void);
void GreenLed_OFF(void);
void RedLed_OFF(void);
void GreenLed_Toggle(void);
void RedLed_Toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LED_H */
