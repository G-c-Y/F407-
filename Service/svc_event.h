#ifndef SVC_EVENT_H
#define SVC_EVENT_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*当前事件类型枚举*/
typedef enum {
    APP_EVNET_NONE = 0,
    APP_EVENT_KEY0,
    APP_EVENT_KEY1,
    APP_EVENT_KEY2,
    APP_EVENT_NET_CHANGED,
    APP_EVENT_DATA_READY,
    APP_EVENT_CACHE_FLUSH
} AppEventType_t;

/*事件结构体*/
typedef struct {
    AppEventType_t type; // 事件类型
    uint32_t parameter; // 事件附带的参数，例如错误码、按键值或计数
} AppEvent_t;

/*函数*/
void SvcEvent_Init(void);//初始化事件服务，清空待处理事件
uint8_t SvcEvent_Post(AppEvent_t event);//在任务上下文发布一个事件
uint8_t SvcEvent_PostFromISR(AppEvent_t event);//在中断上下文发布一个事件；只能做短小、非阻塞操作
uint8_t SvcEvent_Get(AppEvent_t *event);//取出一个待处理事件，复制到调用者变量
void SvcEvent_Clear(void);//放弃当前待处理事件并恢复为空


#ifdef __cplusplus
}
#endif

#endif





