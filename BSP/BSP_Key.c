#include "BSP_Key.h"

bool key0Pressed = false;
bool key1Pressed = false;
bool key2Pressed = false;
 uint32_t last_time = 0;
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    
    if(GPIO_Pin == KEY_PIN0)
    {
        uint32_t now_time = HAL_GetTick();  
        /* 检查引脚是否真的被按下（低电平） */
        if (HAL_GPIO_ReadPin(KEY_PORT, KEY_PIN0) == GPIO_PIN_RESET)
        {
            if(now_time - last_time > 300)
            {
                key0Pressed = true;
            }
        }
         last_time = now_time ;
    }
    else if(GPIO_Pin == KEY_PIN1)
    {
        if (HAL_GPIO_ReadPin(KEY_PORT, KEY_PIN1) == GPIO_PIN_RESET)
        {
            key1Pressed = true;
        }
    }
    else if(GPIO_Pin == KEY_PIN2)
    {
        if (HAL_GPIO_ReadPin(KEY_PORT, KEY_PIN2) == GPIO_PIN_RESET)
        {
            key2Pressed = true;
        }
    }
}
