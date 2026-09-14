#ifndef SVC_STATE_H
#define SVC_STATE_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/*当前系统状态*/
typedef enum {
    SYSTEM_STATE_OFFLINE = 0, // 初始状态，未连接网络
    SYSTEM_STATE_ONLINE , 
    SYSTEM_STATE_CACHING , //正在写入或处理离线缓存
    SYSTEM_STATE_ERROR ,
} SystemState_t;

void SvcState_Init(void);//初始化状态服务，设置为初始状态
void SvcState_Set(SystemState_t state);//直接设置系统状态
SystemState_t SvcState_Get(void);//获取当前系统状态
void SvcState_ToggleOnline(void);//在线和离线之间切换,使用KEY0来模拟离线与在线切换
bool SvcState_IsOnline(void);//返回当前是否在线，返回 1 表示在线
bool SvcState_IsOFFline(void);//返回当前是否离线，返回 1 表示离线


#ifdef __cplusplus
}
#endif

#endif






