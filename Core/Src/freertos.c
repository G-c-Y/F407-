/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "TaskHandle.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for UITASK */
osThreadId_t UITASKHandle;
const osThreadAttr_t UITASK_attributes = {
  .name = "UITASK",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for modbusTask */
osThreadId_t modbusTaskHandle;
const osThreadAttr_t modbusTask_attributes = {
  .name = "modbusTask",
  .stack_size = 768 * 4,
  .priority = (osPriority_t) osPriorityNormal1,
};
/* Definitions for atTask */
osThreadId_t atTaskHandle;
const osThreadAttr_t atTask_attributes = {
  .name = "atTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal7,
};
/* Definitions for mqttTask */
osThreadId_t mqttTaskHandle;
const osThreadAttr_t mqttTask_attributes = {
  .name = "mqttTask",
  .stack_size = 768 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal7,
};
/* Definitions for dogTask */
osThreadId_t dogTaskHandle;
const osThreadAttr_t dogTask_attributes = {
  .name = "dogTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for dataQueue */
osMessageQueueId_t dataQueueHandle;
const osMessageQueueAttr_t dataQueue_attributes = {
  .name = "dataQueue"
};
/* Definitions for lvglDataQueue */
osMessageQueueId_t lvglDataQueueHandle;
const osMessageQueueAttr_t lvglDataQueue_attributes = {
  .name = "lvglDataQueue"
};
/* Definitions for usart1Mutex */
osMutexId_t usart1MutexHandle;
const osMutexAttr_t usart1Mutex_attributes = {
  .name = "usart1Mutex"
};
/* Definitions for flashMutex */
osMutexId_t flashMutexHandle;
const osMutexAttr_t flashMutex_attributes = {
  .name = "flashMutex"
};
/* Definitions for netEvent */
osSemaphoreId_t netEventHandle;
const osSemaphoreAttr_t netEvent_attributes = {
  .name = "netEvent"
};
/* Definitions for keyEvent */
osSemaphoreId_t keyEventHandle;
const osSemaphoreAttr_t keyEvent_attributes = {
  .name = "keyEvent"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
extern void StartUITask(void *argument);
extern void StartModbusTask(void *argument);
extern void StartATTask(void *argument);
extern void StartMQTTTask(void *argument);
extern void StartDogTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of usart1Mutex */
  usart1MutexHandle = osMutexNew(&usart1Mutex_attributes);

  /* creation of flashMutex */
  flashMutexHandle = osMutexNew(&flashMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of netEvent */
  netEventHandle = osSemaphoreNew(1, 0, &netEvent_attributes);

  /* creation of keyEvent */
  keyEventHandle = osSemaphoreNew(1, 0, &keyEvent_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of dataQueue */
  dataQueueHandle = osMessageQueueNew (1, sizeof(GatewayData_t), &dataQueue_attributes);

  /* creation of lvglDataQueue */
  lvglDataQueueHandle = osMessageQueueNew (1, sizeof(GatewayData_t), &lvglDataQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of UITASK */
  UITASKHandle = osThreadNew(StartUITask, NULL, &UITASK_attributes);

  /* creation of modbusTask */
  modbusTaskHandle = osThreadNew(StartModbusTask, NULL, &modbusTask_attributes);

  /* creation of atTask */
  atTaskHandle = osThreadNew(StartATTask, NULL, &atTask_attributes);

  /* creation of mqttTask */
  mqttTaskHandle = osThreadNew(StartMQTTTask, NULL, &mqttTask_attributes);

  /* creation of dogTask */
  dogTaskHandle = osThreadNew(StartDogTask, NULL, &dogTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* configASSERT 断言失败的落点：宏行下不了断点，改在函数体内下断点。
 * 崩的时候会停在这里，从 Call Stack 往上能看到是哪一次 RTOS 调用
 * （osMessageQueuePut / osMutexRelease / osSemaphoreAcquire …）
 * 因为句柄为空或参数非法触发了断言。 */
void vAssertCalled(const char *pcFile, unsigned long ulLine)
{
    (void)pcFile;
    (void)ulLine;
    for (;;)
    {
        /* 断点下在这里：查看 Call Stack 定位触发断言的调用 */
    }
}

/* 任务栈溢出钩子：当 configCHECK_FOR_STACK_OVERFLOW 打开、某任务把栈写穿时，
 * FreeRTOS 会在这里停住。用调试器看 pcTaskName 就是越界的那个任务名；
 * 若在 RAM 有限场合也可在这里点一盏红灯提示。 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    for (;;)
    {
        /* 停在此处：查看 pcTaskName 确认是哪个任务栈溢出 */
    }
}

/* USER CODE END Application */

