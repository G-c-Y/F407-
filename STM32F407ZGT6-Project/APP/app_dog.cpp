#include "app_dog.h"
#include "BSP_Led.h"
#include "iwdg.h"
#include "cmsis_os2.h"

//看门狗刷新成功计数（喂狗次数）
static uint32_t dog_refresh_count;
//看门狗错误计数（系统检查失败次数）
static uint32_t dog_error_count;

//看门狗任务：每秒检查系统状态并喂狗，红灯闪烁指示工作状态
void App_DogTask(void *argument)
{
    (void)argument;  //避免编译器警告

    for (;;)  //无限循环
    {
        //检查系统是否正常（当前始终返回1，可根据需要扩展）
        if (App_Dog_CheckSystem() != 0U)
        {
            HAL_IWDG_Refresh(&hiwdg);  //喂狗，重置看门狗计数器
            /* 后续接入 DvcIWDG_Refresh。当前可只闪烁红灯或累计计数。 */
            dog_refresh_count++;  //喂狗成功计数+1
        }
        else
        {
            dog_error_count++;  //系统检查失败计数+1
        }

        RedLed_Toggle();  //红灯状态切换（亮/灭交替）
        osDelay(1000U);   //1秒延时
    }
}

//系统健康检查函数（可扩展为检查多个系统状态）
//当前实现为简单版本，始终返回1表示系统正常
//可扩展功能：
// - 检查各个FreeRTOS任务的心跳信号
// - 检查队列丢弃数量（如果队列满了可能会丢数据）
// - 检查设备错误计数（SPI/I2C/UART等通信错误）
// - 检查内存使用情况（堆栈溢出检测）
uint8_t App_Dog_CheckSystem(void)
{
    /* 检查任务心跳、队列丢弃数和设备错误数。 */
    return 1U;  //1=系统正常，0=系统异常
}

//获取看门狗刷新成功次数
//返回：累计喂狗次数
uint32_t App_Dog_GetRefreshCount(void)
{
    return dog_refresh_count;
}
