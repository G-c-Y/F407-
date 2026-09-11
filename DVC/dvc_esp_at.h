#ifndef DVC_ESP_AT_H
#define DVC_ESP_AT_H

#include "stm32f4xx_hal.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

//ESP AT指令执行结果枚举
typedef enum
{
    ESP_AT_RESULT_OK = 0,       // AT指令执行成功
    ESP_AT_RESULT_ERROR,        // AT指令执行失败（语法错误、模块无响应等）
    ESP_AT_RESULT_TIMEOUT,      // AT指令超时
    ESP_AT_RESULT_BUSY          // ESP模块忙（收到busy p...，会自动重试）
} EspAtResult_t;

//WIFI和TCP/MQTT连接状态枚举
typedef enum
{
    ESP_NET_OFFLINE = 0,        // ESP处于离线状态（未连接）
    ESP_NET_WIFI_CONNECTING,    // 正在连接WIFI
    ESP_NET_WIFI_READY,         // WIFI已连接
    ESP_NET_TCP_CONNECTING,     // 正在建立TCP连接
    ESP_NET_MQTT_CONNECTING,    // 正在连接MQTT服务器
    ESP_NET_MQTT_READY          // MQTT连接已建立就绪
} EspNetState_t;

#ifndef ESP_WIFI_SSID
#define ESP_WIFI_SSID          "000"
#endif
#ifndef ESP_WIFI_PASSWORD
#define ESP_WIFI_PASSWORD     "12345678" 
#endif
#ifndef ESP_MQTT_HOST
#define ESP_MQTT_HOST       "10.90.70.168"
#endif
#ifndef ESP_MQTT_PORT
#define ESP_MQTT_PORT       1883U
#endif
#ifndef ESP_MQTT_CLIENT_ID  
#define ESP_MQTT_CLIENT_ID  "gw_f407_01" 
#endif
#ifndef ESP_MQTT_USERNAME
#define ESP_MQTT_USERNAME    ""
#endif
#ifndef ESP_MQTT_PASSWORD
#define ESP_MQTT_PASSWORD    ""
#endif
#ifndef ESP_MQTT_TOPIC
#define ESP_MQTT_TOPIC       "stm32"
#endif

//ESP-AT模块初始化，包含ESP-01S模块的AT指令握手和网络初始化
void EspAT_Init(void);
//ESP-AT模块状态更新，轮询网络状态和AT响应
void EspAT_Update(void);
//获取当前ESP网络状态
EspNetState_t EspAT_GetNetworkState(void);
//检查ESP是否已连接MQTT就绪状态
uint8_t EspAT_IsReady(void);
//获取最近一次AT指令失败的阶段信息（用于排障）
const char *EspAT_GetLastStage(void);
//获取最近一次AT指令失败的ESP响应信息（用于排障）
const char *EspAT_GetLastResponse(void);
//通过MQTT发布JSON格式数据（温度、湿度、光照）
EspAtResult_t EspAT_PublishJson(const char *json, uint16_t length);
//通过MQTT发布格式化数据（浮点温度、湿度）
EspAtResult_t EspAT_PublishData(float temperature, float humidity);

#ifdef __cplusplus
}
#endif

#endif /* DVC_ESP_AT_H */
