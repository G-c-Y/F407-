#ifndef SVC_DATA_H
#define SVC_DATA_H

#include "stm32f4xx_hal.h"
#include "dvc_modbus.h"
#include <stdbool.h>
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/*数据是否有效宏定义*/
#define GATEWAY_DATA_VALID_TEMP  (1 << 0) // 温度有效
#define GATEWAY_DATA_VALID_HUMIDITY  (1 << 1)// 湿度有效
#define GATEWAY_DATA_VALID_LIGHT  (1 << 2)// 光照有效

/*网关数据来源宏定义*/
#define GATEWAY_SOURCE_MODBUS  (1 << 0) // 数据来自 Modbus
#define GATEWAY_SOURCE_MQTT  (1 << 1) // 数据来自 MQTT

/*网关数据结构体*/ 
typedef struct{
    float temperature; // 温度
    float humidity; // 湿度
    float light; // 光照
    uint32_t sequence; // 数据序号
    uint32_t timestamp; // 时间戳
    uint8_t valid_mask; // 有效字段标记
    uint8_t source_mask; // 数据来源标记
}GatewayData_t; //ai补全有读心术吗？？！

// /*来自Modbus传感器的数据结构体*/
// typedef struct{
//     float temperature; // 温度
//     float humidity; // 湿度
//     float temperture_valid; // 温度有效标记
//     float humidity_valid; // 湿度有效标记
// }ModbusData_t;

/*来自MQTT的数据结构体*/
typedef struct{
    float light; // 光照
    uint32_t sequence; // 数据序号
    uint32_t timestamp; // 时间戳
}MqttData_t;

/*函数接口*/
void SvcData_Reset(GatewayData_t *data);//将提供的数据清零
bool SvcData_IsValid(const GatewayData_t *data);//检查数据是否在合理范围，是否有效
void SvcData_UpdateModbus(const ModbusData_t *update);//更新MODBUS数据，只更新温湿度，光照保持不变
void SvcData_UpdateMqtt(const MqttData_t *update);//更新MQTT数据，只更新光照、序号和时间戳，温湿度保持不变
void SvcData_GetLatest(GatewayData_t *data);//获取最新网关数据
bool SvcData_SubmitLatest(void);//提交最新数据，把完整最新数据复制到dataQueueHandle，成功则增加 submit_count，失败则增加drop_count
uint32_t SvcData_GetSubmitCount(void);//返回成功提交次数
uint32_t SvcData_GetDropCount(void);//返回丢弃次数

#ifdef __cplusplus
}
#endif

#endif /* SVC_DATA_H */
