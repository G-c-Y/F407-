#include "app_mqtt.h"
#include "TaskHandle.h"
#include "svc_cache.h"
#include "svc_state.h"
#include "dvc_esp_at.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>

//MQTT JSON消息缓冲区大小
#define MQTT_JSON_BUFFER_SIZE 192U

//静态变量：接收到的数据
static GatewayData_t received_data;
//MQTT统计数据：上传成功数、接收总数、发布总数、错误总数
uint32_t mqtt_uploaded_count = 0U;  //成功上传的数据计数
uint32_t mqtt_receive_count = 0U;   //从dataQueue接收到的数据总数
uint32_t mqtt_publish_count = 0U;   //MQTT发布尝试总数
uint32_t mqtt_error_count = 0U;     //MQTT发布错误总数

//回放事务状态机变量：控制离线数据回放流程
static uint8_t  mqtt_replay_started = 0U;    //回放开始标志：0=未开始，1=已发送开始信号
static uint32_t mqtt_replay_total = 0U;     //本次待回放的总记录数

//发送JSON格式数据到MQTT
//参数：json - 要发送的JSON字符串
//返回：1=成功，0=失败
static uint8_t App_MQTT_SendJson(const char *json)
{
    uint16_t length;  //JSON字符串长度

    //参数检查：JSON字符串为空则返回失败
    if (json == NULL)
    {
        return 0U;
    }
    length = (uint16_t)strlen(json);  //计算字符串长度
    //调用ESP-AT模块发送JSON数据，返回发送结果
    return (EspAT_PublishJson(json, length) == ESP_AT_RESULT_OK) ? 1U : 0U;
}

//发送离线回放开始信号
//参数：count - 本次要回放的数据记录总数
//返回：1=发送成功，0=失败
static uint8_t App_MQTT_SendReplayStart(uint32_t count)
{
    char json[MQTT_JSON_BUFFER_SIZE];  //JSON消息缓冲区

    //构建JSON格式的开始信号：{"type":"offline_replay","event":"start","count":N}
    (void)sprintf(json,
                  "{\"type\":\"offline_replay\","
                  "\"event\":\"start\",\"count\":%lu}",
                  (unsigned long)count);
    return App_MQTT_SendJson(json);  //发送JSON并返回结果
}

//发送离线回放完成信号
//参数：count - 本次成功回放的数据记录总数
//返回：1=发送成功，0=失败
static uint8_t App_MQTT_SendReplayFinish(uint32_t count)
{
    char json[MQTT_JSON_BUFFER_SIZE];  //JSON消息缓冲区

    //构建JSON格式的完成信号：{"type":"offline_replay","event":"finish","count":N}
    (void)sprintf(json,
                  "{\"type\":\"offline_replay\","
                  "\"event\":\"finish\",\"count\":%lu}",
                  (unsigned long)count);
    return App_MQTT_SendJson(json);  //发送JSON并返回结果
}

static uint8_t App_MQTT_SendOfflineData(const GatewayData_t *data,
                                        uint32_t sequence)
{
    char json[MQTT_JSON_BUFFER_SIZE];
    int temperature10;
    int humidity10;
    int light10;
    int length;
    int temperature_fraction;
    int humidity_fraction;
    int light_fraction;

    if (data == NULL)
    {
        return 0U;
    }

    temperature10 = (int)(data->temperature * 10.0f +
                          ((data->temperature >= 0.0f) ? 0.5f : -0.5f));
    humidity10 = (int)(data->humidity * 10.0f + 0.5f);
    light10 = (int)(data->light * 10.0f +
                    ((data->light >= 0.0f) ? 0.5f : -0.5f));
    temperature_fraction = temperature10 % 10;
    humidity_fraction = humidity10 % 10;
    light_fraction = light10 % 10;
    if (temperature_fraction < 0) temperature_fraction = -temperature_fraction;
    if (humidity_fraction < 0) humidity_fraction = -humidity_fraction;
    if (light_fraction < 0) light_fraction = -light_fraction;

    length = sprintf(json,
                     "{\"type\":\"offline_data\","
                     "\"sequence\":%lu,\"timestamp\":%lu,"
                     "\"temperature\":%d.%d,\"humidity\":%d.%d,"
                     "\"light\":%d.%d}",
                     (unsigned long)sequence,
                     (unsigned long)data->timestamp,
                     temperature10 / 10, temperature_fraction,
                     humidity10 / 10, humidity_fraction,
                     light10 / 10, light_fraction);
    if ((length <= 0) || ((uint32_t)length >= sizeof(json)))
    {
        return 0U;
    }
    return App_MQTT_SendJson(json);
}

void App_MQTT_ReplayStep(void); /* 分步回放，定义在文件后部 */

//MQTT任务主函数：处理实时数据发布和离线数据回放
void App_MQTTTask(void *argument)
{
    (void)argument;  //避免编译器警告

    for (;;)  //无限循环任务
    {
        //1) 优先消费实时采集数据：在线则实时发布，离线则写缓存，
        //   同时推送 UI 队列，保证回放期间界面 T/H/L 仍刷新
        if (osMessageQueueGet(dataQueueHandle,
                              &received_data,
                              NULL,
                              0U) == osOK)
        {
            mqtt_receive_count++;  //接收计数+1
            (void)App_MQTT_ProcessData(&received_data);  //处理数据（发布或缓存）
        }

        //2) 在线且有离线缓存时，每轮只回放一条（分步），避免长时间占用任务
        App_MQTT_ReplayStep();  //处理离线数据回放（一条）

        osDelay(5U);  //5ms延时，任务循环间隔
    }
}

//处理传感器数据：根据在线状态决定实时发布还是缓存
//参数：data - 网关传感器数据
//返回：1=处理成功（发布或缓存），0=处理失败
uint8_t App_MQTT_ProcessData(const GatewayData_t *data)
{
    uint8_t accepted = 1U;  //是否被接受（发布或缓存）的标志

    //参数检查：数据指针为空或数据无效则计数错误并返回失败
    if ((data == NULL) || !SvcData_IsValid(data))
    {
        mqtt_error_count++;
        return 0U;
    }

    //检查当前是否在线（已连接MQTT服务器）
    if (SvcState_IsOnline())
    {
        //在线：直接发布数据到MQTT
        if (EspAT_PublishData(data->temperature, data->humidity) == ESP_AT_RESULT_OK)
        {
            mqtt_publish_count++;  //发布成功计数+1
        }
        else
        {
            /* 在线发送失败的数据也必须落入离线缓存，确保数据不丢失 */
            accepted = SvcCache_Add(data);  //写入缓存
            mqtt_error_count++;            //错误计数+1
        }
    }
    else
    {
        //离线：直接写入缓存，不尝试发布
        accepted = SvcCache_Add(data);
    }

    /* UI队列只保留最新数据显示，队列满时不影响MQTT/Flash主流程
     * 注意：即使是失败数据也会推送到UI（数据有效性已在前面检查） */
    (void)osMessageQueuePut(lvglDataQueueHandle, data, 0U, 0U);
    if (accepted == 0U)
    {
        mqtt_error_count++;  //缓存失败也要计数
    }
    return accepted;
}

uint8_t App_MQTT_HandleSensorMessage(char *json, uint32_t length)
{
    (void)json;
    (void)length;
    return 0U;
}

uint8_t App_MQTT_HandleControlMessage(char *json, uint32_t length)
{
    (void)json;
    (void)length;
    return 0U;
}

//离线数据回放一步：每次只处理一条记录，实现非阻塞回放
//回放流程：Peek -> 发布 -> 成功才Commit -> 推送UI更新
void App_MQTT_ReplayStep(void)
{
    GatewayData_t cached_data;  //从缓存读取的数据
    uint32_t sequence;          //数据序列号

    //不在线：清掉进行中的回放事务，离线数据留在Flash等待下次联网
    if (!SvcState_IsOnline())
    {
        mqtt_replay_started = 0U;  //重置开始标志
        mqtt_replay_total = 0U;   //重置总数
        return;
    }

    //缓存已空：若开始信号已发过，则补发完成信号收尾
    if (SvcCache_Count() == 0U)
    {
        if (mqtt_replay_started != 0U)  //已发送开始信号
        {
            if (App_MQTT_SendReplayFinish(mqtt_replay_total) == 0U)
            {
                /* finish失败不能伪造完成，保留状态待下次重发 */
                mqtt_error_count++;
                return;
            }
            mqtt_replay_started = 0U;  //重置开始标志
            mqtt_replay_total = 0U;   //重置总数
        }
        return;
    }

    //新事务：先发开始信号，失败则不动缓存
    if (mqtt_replay_started == 0U)  //尚未开始回放
    {
        mqtt_replay_total = SvcCache_Count();  //获取缓存总数
        if (App_MQTT_SendReplayStart(mqtt_replay_total) == 0U)
        {
            mqtt_error_count++;  //发送开始信号失败
            return;
        }
        mqtt_replay_started = 1U;  //标记已发送开始信号
    }

    //每轮只回放一条：Peek -> 发布 -> 成功才Commit（保证数据一致性）
    if (SvcCache_Peek(&cached_data, &sequence) == 0U)  //从缓存读取数据
    {
        mqtt_error_count++;
        mqtt_replay_started = 0U;  //读取失败，重置状态
        return;
    }
    if (App_MQTT_SendOfflineData(&cached_data, sequence) == 0U)  //发布离线数据
    {
        /* 发布失败不提交，保留该条及后续，等下一轮重试 */
        mqtt_error_count++;
        return;
    }
    if (SvcCache_CommitPeek() == 0U)  //成功提交（从缓存删除）
    {
        mqtt_error_count++;
        return;
    }
    mqtt_publish_count++;  //发布成功计数+1

    /* 顺带把离线数据推给UI，让界面跟随回放内容刷新
     * 这样回放过程中，UI显示的是实际正在发送的数据 */
    (void)osMessageQueuePut(lvglDataQueueHandle, &cached_data, 0U, 0U);
}

//兼容函数：离线数据批量回放（一次性处理，直到清空或掉线）
//注意：此函数会阻塞直到处理完或超时，建议使用App_MQTT_ReplayStep分步回放
uint8_t App_MQTT_ReplayCache(void)
{
    /* 兼容入口：连发直到清空或掉线（供外部一次性调用）。
     * guard < 4000 防止无限循环（约4000条记录，预估超时几十秒） */
    uint32_t guard = 0U;  //循环计数器，防死锁

    //循环条件：在线 + 缓存不为空 + 未超过最大循环次数
    while ((SvcState_IsOnline()) && (SvcCache_Count() != 0U) &&
           (guard < 4000U))
    {
        App_MQTT_ReplayStep();  //每步处理一条记录
        guard++;
    }
    //返回：1=缓存已清空，0=因网络断开或超时未清空
    return (SvcCache_Count() == 0U) ? 1U : 0U;
}

//本地模拟数据生成函数（当前未使用）
//注：采集数据由ModbusTask提供，此函数保留用于将来可能的本地数据模拟
void App_MQTT_GenerateLocalSimulation(void)
{
    /* 采集数据由 ModbusTask 提供。 */
}
