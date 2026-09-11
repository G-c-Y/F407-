#ifndef APP_DOG_H
#define APP_DOG_H

#include "stm32f4xx_hal.h"
#include "stdint.h"


#ifdef __cplusplus
extern "C"{
#endif

void App_DogTask(void *argument);
uint8_t App_Dog_CheckSystem(void);
uint32_t App_Dog_GetRefreshCount(void);

#ifdef __cplusplus
}
#endif

#endif
