#ifndef APP_AI_H
#define APP_AI_H

#include "stm32f4xx_hal.h"
#include "svc_data.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AI 输出状态类别（与训练脚本 train_sensor_anomaly.py 的三类一一对应） */
#define AI_STATE_NORMAL   0U   /* 0：正常 */
#define AI_STATE_WARNING  1U   /* 1：预警（数值持续漂移） */
#define AI_STATE_FAULT    2U   /* 2：故障（突跳 / 传感器卡死） */

/* 一次推理的结果快照（供 UI 读取显示） */
typedef struct
{
    uint8_t state;            /* AI_STATE_xxx，取输出概率最大的类别 */
    float score;              /* 该类别的归一化概率 0.0~1.0 */
    uint32_t run_count;       /* 累计推理次数 */
    uint32_t error_count;     /* 推理失败次数（Cube.AI 返回异常） */
} AiResult_t;

/* 每次拿到新的传感器数据时调用：喂入滑动窗口（在 UI 任务里调用） */
void App_AI_Feed(const GatewayData_t *data);

/* 周期性推理入口：内部按间隔节流，每 2 秒真正执行一次推理
 * 返回 1 表示本次产生了新结果，result 已更新；返回 0 表示未到周期或未就绪 */
uint8_t App_AI_Process(AiResult_t *result);

/* 读取最近一次推理结果（随时可调，无锁） */
AiResult_t App_AI_GetResult(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_AI_H */
