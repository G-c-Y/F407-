#include "app_at.h"
#include "Bsp_Key.h"
#include "TaskHandle.h"
#include "svc_state.h"
#include "dvc_esp_at.h"
#include "cmsis_os2.h"

static uint32_t key0_count = 0U;
static uint32_t key1_count = 0U;
static uint32_t key2_count = 0U;
static uint32_t network_check_count = 0U;
static uint32_t esp_reconnect_tick = 0U;
static uint8_t esp_was_ready = 0U;

#define ESP_RECONNECT_PERIOD_MS 10000U

void App_ATTask(void *argument)
{
    (void)argument;

    EspAT_Init();
    esp_reconnect_tick = HAL_GetTick();

    for (;;)
    {
        if (key0Pressed)
        {
            key0Pressed = false;
           // App_AT_HandleKey0();
        }

        if (key1Pressed)
        {
            key1Pressed = false;
            App_AT_HandleKey1();
        }

        if (key2Pressed)
        {
            key2Pressed = false;
            App_AT_HandleKey2();
        }

        App_AT_UpdateNetworkState();
        osDelay(50U);
    }
}

void App_AT_HandleKey0(void)
{
    key0_count++;
    SvcState_ToggleOnline();
    (void)osSemaphoreRelease(netEventHandle);
}

void App_AT_HandleKey1(void)
{
    key1_count++;
}

void App_AT_HandleKey2(void)
{
    key2_count++;
}

void App_AT_UpdateNetworkState(void)
{
    uint8_t ready;

    network_check_count++;

    /* 心跳保活 + 掉线检测（仅就绪后生效） */
    EspAT_Update();

    ready = EspAT_IsReady();
    if (ready != esp_was_ready)
    {
        esp_was_ready = ready;
        if (ready)
        {
            SvcState_Set(SYSTEM_STATE_ONLINE);
            (void)osSemaphoreRelease(netEventHandle);
        }
        else
        {
            SvcState_Set(SYSTEM_STATE_OFFLINE);
        }
    }

    /* 掉线后按固定周期重试整条链路，避免反复失败频繁阻塞该任务 */
    if ((ready == 0U) && (EspAT_GetNetworkState() == ESP_NET_OFFLINE) &&
        ((HAL_GetTick() - esp_reconnect_tick) >= ESP_RECONNECT_PERIOD_MS))
    {
        esp_reconnect_tick = HAL_GetTick();
        EspAT_Init();
    }
}
