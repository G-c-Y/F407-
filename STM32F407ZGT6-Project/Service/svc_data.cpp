#include "svc_data.h"
#include "TaskHandle.h"
#include "cmsis_os2.h"
#include <string.h>

static GatewayData_t latest_data = 
{
    25.0f,
    60.0f,
    100.0f,
    0U,
    0U,
    GATEWAY_DATA_VALID_TEMP |
    GATEWAY_DATA_VALID_HUMIDITY |
    GATEWAY_DATA_VALID_LIGHT,
    GATEWAY_SOURCE_MODBUS
};

static uint32_t submit_count = 0U;
static uint32_t drop_count = 0U;

void SvcData_Reset(GatewayData_t *data)
{
    if (data != NULL)
    {
        memset(data, 0, sizeof(GatewayData_t));
    }
}

bool SvcData_IsValid(const GatewayData_t *data)
{
    if (data == NULL)
    {
        return false;
    }

    if ((data->valid_mask & GATEWAY_DATA_VALID_TEMP) != 0U &&
        (data->temperature < -40.0f || data->temperature > 125.0f))
    {
        return false;
    }

    if ((data->valid_mask & GATEWAY_DATA_VALID_HUMIDITY) != 0U &&
        (data->humidity < 0.0f || data->humidity > 100.0f))
    {
        return false;
    }

    if ((data->valid_mask & GATEWAY_DATA_VALID_LIGHT) != 0U &&
        data->light < 0.0f)
    {
        return false;
    }

    return ((data->valid_mask & GATEWAY_DATA_VALID_TEMP) != 0U) &&
           ((data->valid_mask & GATEWAY_DATA_VALID_HUMIDITY) != 0U);
}

void SvcData_UpdateModbus(const ModbusData_t *update)
{
    if (update == NULL)
    {
        return;
    }

    if (update->temperture_valid != 0.0f)
    {
        latest_data.temperature = update->temperature;
        latest_data.valid_mask |= GATEWAY_DATA_VALID_TEMP;
    }

    if (update->humidity_valid != 0.0f)
    {
        latest_data.humidity = update->humidity;
        latest_data.valid_mask |= GATEWAY_DATA_VALID_HUMIDITY;
    }

    latest_data.source_mask |= GATEWAY_SOURCE_MODBUS;
    latest_data.sequence++;
}

void SvcData_UpdateMqtt(const MqttData_t *update)
{
    if (update == NULL)
    {
        return;
    }

    latest_data.light = update->light;
    latest_data.sequence = update->sequence;
    latest_data.timestamp = update->timestamp;
    latest_data.valid_mask |= GATEWAY_DATA_VALID_LIGHT;
    latest_data.source_mask |= GATEWAY_SOURCE_MQTT;
}

void SvcData_GetLatest(GatewayData_t *data)
{
    if (data != NULL)
    {
        memcpy(data, &latest_data, sizeof(GatewayData_t));
    }
}

bool SvcData_SubmitLatest(void)
{
    GatewayData_t snapshot;

    if (!SvcData_IsValid(&latest_data))
    {
        drop_count++;
        return false;
    }

    memcpy(&snapshot, &latest_data, sizeof(GatewayData_t));
    if (osMessageQueuePut(dataQueueHandle, &snapshot, 0U, 0U) == osOK)
    {
        submit_count++;
        return true;
    }

    drop_count++;
    return false;
}

uint32_t SvcData_GetSubmitCount(void)
{
    return submit_count;
}

uint32_t SvcData_GetDropCount(void)
{
    return drop_count;
}
