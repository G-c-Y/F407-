#ifndef APP_UI_H
#define APP_UI_H

#include "stm32f4xx_hal.h"
#include "svc_data.h"
#include "svc_state.h"

#ifdef __cplusplus
extern "C"{
#endif

void App_UITask(void *argument);                       /* UI 任务入口 */
void App_UI_CreateMainPage(void);                      /* 创建 LVGL 主页面 */
void App_UI_UpdateData(const GatewayData_t *data);     /* 刷新温湿度光照 */
void App_UI_UpdateState(SystemState_t state);          /* 刷新网络状态 */
void App_UI_UpdateCacheCount(uint32_t count);          /* 刷新缓存条数 */
void App_UI_UpdateAiState(uint8_t state, uint8_t valid); /* 刷新AI判断显示 */

#ifdef __cplusplus
}
#endif

#endif
