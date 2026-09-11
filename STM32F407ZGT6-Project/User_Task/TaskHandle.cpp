#include "TaskHandle.h"
#include "app_modbus.h"
#include "app_mqtt.h"
#include "app_ui.h"
#include "app_at.h"
#include "app_dog.h"

extern "C" void StartModbusTask(void *argument)
{
    App_ModbusTask(argument);
}

extern "C" void StartMQTTTask(void *argument)
{
    App_MQTTTask(argument);
}

extern "C" void StartUITask(void *argument)
{
    App_UITask(argument);
}

extern "C" void StartATTask(void *argument)
{
    App_ATTask(argument);
}

extern "C" void StartDogTask(void *argument)
{
    App_DogTask(argument);
}
