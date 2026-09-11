#include "app_modbus.h"
#include "cmsis_os2.h"

//Modbus采集成功计数（统计正常工作的次数）
static uint32_t modbus_collect_count = 0U;
//Modbus错误计数（统计出现错误的次数）
static uint32_t modbus_error_count = 0U;

//Modbus应用任务：周期性采集传感器数据并推送到队列
void App_ModbusTask(void *argument)
{
    ModbusData_t update_data;  //用于存储采集到的数据
    (void)argument;  //避免编译器警告

    for (;;)
    {
        //采集数据
        if (App_ModbusCollect(&update_data) != 0U)
        {
            //采集成功：更新Modbus数据并提交到全局数据队列
            SvcData_UpdateModbus(&update_data);
            (void)SvcData_SubmitLatest();  //提交最新数据到MQTT任务
            modbus_collect_count++;  //成功计数+1
        }
        else
        {
            //采集失败：处理错误情况
            App_ModbusHandleError();
        }

        //延时800ms（采集周期可根据实际传感器调整）
        osDelay(800U);
    }
}

//Modbus数据采集：从传感器读取最新数据
//注：当前使用模拟数据，实际使用时取消注释Modbus_dataGet(update)
uint8_t App_ModbusCollect(ModbusData_t *update)
{
    static float simulated_temperature = 25.0f;  //模拟温度值
    //参数检查
    if (update == NULL)
    {
        return 0U;
    }

    /* 驱动层负责UART通信、CRC校验和范围验证
     * 实际使用时取消下面的模拟数据代码，启用真实读取：
     */
    return Modbus_dataGet(update);
    //使用模拟数据（当没有真实传感器时使用）
    // update->temperature = simulated_temperature;  //模拟温度
    // update->humidity = 60.0f;                   //固定湿度值
    // update->temperture_valid = 1.0f;            //温度有效
    // update->humidity_valid = 1.0f;             //湿度有效

    // //模拟温度变化（25°C -> 30°C 循环）
    // simulated_temperature += 0.1f;
    // if (simulated_temperature > 30.0f)
    // {
    //     simulated_temperature = 25.0f;  //重置到25°C
    // }

    //return 1U;  //返回成功（模拟数据总是成功的）
}

//Modbus错误处理：处理Modbus通信错误和异常情况
void App_ModbusHandleError(void)
{
    //错误计数+1（可用于统计和分析错误频率）
    modbus_error_count++;
}
