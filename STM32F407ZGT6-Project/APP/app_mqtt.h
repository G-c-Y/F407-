#ifndef APP_MQTT_H
#define APP_MQTT_H

#include <stdint.h>
#include "svc_data.h"

#ifdef __cplusplus
extern "C" {
#endif

void App_MQTTTask(void *argument);
uint8_t App_MQTT_ProcessData(const GatewayData_t *data);
uint8_t App_MQTT_HandleSensorMessage(char *json, uint32_t length);
uint8_t App_MQTT_HandleControlMessage(char *json, uint32_t length);
uint8_t App_MQTT_ReplayCache(void);
void App_MQTT_GenerateLocalSimulation(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MQTT_H */
