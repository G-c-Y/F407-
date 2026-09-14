#ifndef TASKHANDLE_H
#define TASKHANDLE_H

#include "cmsis_os2.h"
#include "svc_data.h"

#ifdef __cplusplus
extern "C" {
#endif

extern osMessageQueueId_t dataQueueHandle;
extern osMessageQueueId_t lvglDataQueueHandle;
extern osMutexId_t usart1MutexHandle;
extern osMutexId_t flashMutexHandle;
extern osSemaphoreId_t netEventHandle;

void StartDefaultTask(void *argument);
void StartUITask(void *argument);
void StartModbusTask(void *argument);
void StartATTask(void *argument);
void StartMQTTTask(void *argument);
void StartDogTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* TASKHANDLE_H */
