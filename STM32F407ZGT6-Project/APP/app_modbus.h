#ifndef APP_MODBUS_H
#define APP_MODBUS_H

#include <stdint.h>
#include "svc_data.h"
#include "dvc_modbus.h"

#ifdef __cplusplus
extern "C" {
#endif

//Modbus应用任务：周期性采集传感器数据并推送到队列
void App_ModbusTask(void *argument);
//Modbus数据采集：从传感器读取最新数据
//参数：update - 指向ModbusData_t结构体的指针，用于存储采集的数据
//返回：1=采集成功，0=采集失败
uint8_t App_ModbusCollect(ModbusData_t *update);
//Modbus错误处理：处理Modbus通信错误和异常情况
void App_ModbusHandleError(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MODBUS_H */
