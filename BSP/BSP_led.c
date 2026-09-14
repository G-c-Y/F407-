#include "BSP_Led.h"

void GreenLed_ON(void)
{
    HAL_GPIO_WritePin(LED_PORT, GREEN_LED_PIN, GPIO_PIN_RESET);//低电平点亮
}

void RedLed_ON(void)
{
    HAL_GPIO_WritePin(LED_PORT, RED_LED_PIN, GPIO_PIN_RESET);//低电平点亮
}

void GreenLed_OFF(void)
{
    HAL_GPIO_WritePin(LED_PORT, GREEN_LED_PIN, GPIO_PIN_SET);//高电平熄灭
}
void RedLed_OFF(void)
{
    HAL_GPIO_WritePin(LED_PORT, RED_LED_PIN, GPIO_PIN_SET);//高电平熄灭
}

void GreenLed_Toggle(void)
{
    HAL_GPIO_TogglePin(LED_PORT, GREEN_LED_PIN);
}

void RedLed_Toggle(void)
{
    HAL_GPIO_TogglePin(LED_PORT, RED_LED_PIN);
}
