#ifndef DVC_MODBUS_H
#define DVC_MODBUS_H

#include "stm32f4xx_hal.h"
#include "stdbool.h"
#include "stdint.h"
#include "usart.h"
#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif

//Modbus RTU传感器数据驱动模块

//Modbus RTU通信配置参数
#define MODBUS_SLAVE_ADDRESS       0x01U     //从站地址（传感器地址）
#define MODBUS_FUNCTION_READ       0x03U     //读取保持寄存器功能码
#define MODBUS_START_REGISTER      0x0300U   //起始寄存器地址（温度0300H，湿度0301H）
#define MODBUS_REGISTER_COUNT      2U        //读取的寄存器数量（温度+湿度共2个）
#define MODBUS_REQUEST_LENGTH      8U        //Modbus请求帧长度（标准RTU格式）
#define MODBUS_RESPONSE_LENGTH     9U        //Modbus响应帧长度（标准RTU格式）
#define MODBUS_UART_TIMEOUT_MS     500U      //UART通信超时时间
#define MODBUS_TC_TIMEOUT_MS       10U       //通信线路空闲超时时间


//RS485收发控制：根据硬件设计，PG8=1为发送，PG8=0为接收
#define MODBUS_TX_ENABLE() \
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_8, GPIO_PIN_SET)   //启用发送模式
#define MODBUS_RX_ENABLE() \
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_8, GPIO_PIN_RESET) //启用接收模式

//来自Modbus传感器的数据结构体
typedef struct{
    float temperature;     //温度值（单位：摄氏度）
    float humidity;         //湿度值（单位：百分比RH）
    float temperture_valid; //温度有效标记（1.0=有效，0.0=无效）
    float humidity_valid;  //湿度有效标记（1.0=有效，0.0=无效）
}ModbusData_t;
//Modbus RTU CRC16校验函数（标准RTU协议）
uint16_t Modbus_CRC16(uint8_t *data , uint8_t length);
//直接读取Modbus传感器数据（内部使用）
uint8_t Modbus_readDev(ModbusData_t *data); //获取传感器数据
//外部数据获取接口（提供统一的访问方式）
uint8_t Modbus_dataGet(ModbusData_t *data);//外部数据获取接口

#ifdef __cplusplus
}
#endif

#endif

