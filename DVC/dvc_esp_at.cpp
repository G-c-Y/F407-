#include "dvc_esp_at.h"
#include "TaskHandle.h"
#include "cmsis_os2.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------
 * ESP8266/ESP-01S 最小在线链路驱动（普通 AT + TCP 透传，STM32 手动发 MQTT）
 *
 * 链路：CWMODE -> CWJAP(等 WIFI GOT IP) -> CIPSTART(等 CONNECT)
 *       -> CIPMODE=1 -> CIPSEND(等 '>') -> 发 MQTT CONNECT(等 CONNACK)
 * 之后进入透传：周期 PING 保活，PublishData 直接发送 MQTT PUBLISH。
 *
 * 说明：
 *  - 不依赖 AT+MQTT* 这类定制固件命令，普通 ESP8266 AT 固件即可。
 *  - 状态只在收到关键 URC 后才前进，单靠 OK 不算联网成功。
 *  - 所有大缓冲区静态分配，避免占满 atTask 的小任务栈。
 * ------------------------------------------------------------------ */

//环形缓冲区大小：ESP-01S AT响应缓冲区
#define ESP_RX_BUFFER_SIZE       512U
//AT命令缓冲区大小：单条AT指令最大长度
#define ESP_COMMAND_BUFFER_SIZE  192U
//MQTT数据包缓冲区大小：单条MQTT消息最大长度
#define ESP_PACKET_BUFFER_SIZE   256U
//AT指令响应超时时间
#define ESP_RESPONSE_TIMEOUT_MS  3000U
//WIFI连接超时时间
#define ESP_WIFI_TIMEOUT_MS      15000U
//TCP连接超时时间
#define ESP_TCP_TIMEOUT_MS       10000U
//MQTT心跳保活周期
#define ESP_PING_PERIOD_MS       10000U

#define ESP_RX_BUFFER_SIZE       512U
#define ESP_COMMAND_BUFFER_SIZE  192U
#define ESP_PACKET_BUFFER_SIZE   256U
#define ESP_RESPONSE_TIMEOUT_MS  3000U
#define ESP_WIFI_TIMEOUT_MS      15000U
#define ESP_TCP_TIMEOUT_MS       10000U
#define ESP_PING_PERIOD_MS       10000U

//UART1接收：ISR只负责把字节搬进环形缓冲，解析都放到任务上下文中
static volatile uint8_t esp_rx_byte;              //当前接收的字节
static volatile uint8_t esp_rx_buffer[ESP_RX_BUFFER_SIZE];  //环形接收缓冲区
static volatile uint16_t esp_rx_head;            //环形缓冲区头指针（写入位置）
static volatile uint16_t esp_rx_tail;            //环形缓冲区尾指针（读取位置）

//AT指令暂存缓冲区，用于组装完整的AT命令
static char esp_command[ESP_COMMAND_BUFFER_SIZE];
//MQTT二进制数据包缓冲区，用于组装MQTT协议报文
static uint8_t esp_packet[ESP_PACKET_BUFFER_SIZE];
//ESP响应文本缓冲区，用于匹配期望的响应内容
static char esp_response[ESP_COMMAND_BUFFER_SIZE];
//动态拼接的期望字符串，用于查找特定响应（如"+CWJAP:\"XXX\""）
static char esp_expect[ESP_COMMAND_BUFFER_SIZE];
//JSON格式发布数据缓冲区，用于存储要发布的MQTT payload
static char esp_json[ESP_COMMAND_BUFFER_SIZE];

//ESP网络状态变量，初始化为离线状态
static EspNetState_t esp_state = ESP_NET_OFFLINE;
//上次心跳发送时间戳（用于周期性保活）
static uint32_t esp_last_ping;
//ESP初始化完成标志位
static uint8_t esp_initialized;

//排障相关变量：记录最近一次失败的阶段和响应，供LCD显示定位卡点
static const char *esp_last_stage = "NONE";           //最近一次失败的阶段描述
static char esp_last_response[ESP_COMMAND_BUFFER_SIZE]; //当时ESP返回的最后一段响应

//记录AT指令失败信息，用于排障
static void EspAT_RecordFail(const char *stage)
{
    //记录失败的阶段（如"AT+CWJAP"、"CONNECT"等）
    esp_last_stage = stage;
    //保存当前ESP响应的前n-1个字节，防止越界
    (void)memcpy(esp_last_response, esp_response,
                 sizeof(esp_last_response) - 1U);
    //确保字符串以NULL结尾，防止越界访问
    esp_last_response[sizeof(esp_last_response) - 1U] = '\0';
}

//清空接收缓冲区：只修改tail指针，避免和中断服务程序ISR的head指针冲突
static void EspAT_ClearRx(void)
{
    //临时关闭中断，防止ISR在清空过程中写入新数据
    __disable_irq();
    //将尾指针追上头指针，相当于清空缓冲区
    esp_rx_tail = esp_rx_head;
    //重新开启中断
    __enable_irq();
}

//从环形缓冲区读取一个字节；如果缓冲区为空则返回0
static uint16_t EspAT_ReadByte(uint8_t *value)
{
    //参数检查：value指针为空或缓冲区为空（tail=head）
    if ((value == NULL) || (esp_rx_tail == esp_rx_head))
    {
        return 0U;  //读取失败
    }

    //取出缓冲区尾指针处的字节
    *value = esp_rx_buffer[esp_rx_tail];
    //尾指针向前移动一位，模运算实现环形缓冲区
    esp_rx_tail = (uint16_t)((esp_rx_tail + 1U) % ESP_RX_BUFFER_SIZE);
    return 1U;  //读取成功
}

//ESP忙时会丢弃收到的AT并回"busy p..."（不排队），驱动须自动重发
#define ESP_AT_BUSY_RETRIES       5U     //一条命令最多重发次数
#define ESP_AT_BUSY_RETRY_DELAY_MS 300U  //busy后等待多少毫秒再重发

//在超时内等待期望文本；期间一旦出现ERROR/FAIL立即判定失败
//用滚动窗口做子串匹配，期望串较长时也不会被缓冲上限截断
//返回：1=匹配到期望串，2=收到busy p...（命令被丢弃，可重发），0=超时/ERROR/FAIL
static uint8_t EspAT_WaitText(const char *expected, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();  //开始等待的时间戳
    uint16_t length = 0U;           //当前响应文本长度
    uint8_t byte;                    //从缓冲区读取的字节

    //参数检查：期望字符串为空或为空串则直接返回失败
    if ((expected == NULL) || (expected[0] == '\0'))
    {
        return 0U;
    }

    //清空响应缓冲区
    memset(esp_response, 0, sizeof(esp_response));
    //在超时时间内持续等待
    while ((HAL_GetTick() - start) < timeout_ms)
    {
        //不断从环形缓冲区读取字节
        while (EspAT_ReadByte(&byte) != 0U)
        {
            //如果缓冲区未满，直接追加新字节
            if (length < (sizeof(esp_response) - 1U))
            {
                esp_response[length++] = (char)byte;
                esp_response[length] = '\0';
            }
            else
            {
                //缓冲区已满，采用滚动窗口：移除第一个字节，在新位置追加
                memmove(esp_response, &esp_response[1], sizeof(esp_response) - 2U);
                esp_response[sizeof(esp_response) - 2U] = (char)byte;
                esp_response[sizeof(esp_response) - 1U] = '\0';
            }

            //检查是否匹配到期望的字符串
            if (strstr(esp_response, expected) != NULL)
            {
                return 1U;  //成功匹配期望字符串
            }
            //检查是否收到ERROR或FAIL，立即判定失败
            if ((strstr(esp_response, "ERROR") != NULL) ||
                (strstr(esp_response, "FAIL") != NULL))
            {
                return 0U;  //命令执行失败
            }
            //收到"busy p..."：本条AT已被ESP丢弃（不排队），返回2让上层重发
            if (strstr(esp_response, "busy p...") != NULL)
            {
                return 2U;  //ESP忙，需要重发
            }
        }
        osDelay(1U);  //短暂延时，避免CPU空转
    }
    return 0U;  //超时未匹配到期望字符串
}

//单条AT事务：互斥锁覆盖【发送 + 响应匹配 + 结果等待】全过程
//若收到busy p...（本条命令被ESP丢弃），延时后把同一条命令重发
//只有等不到期望文本且不是busy（超时/ERROR/FAIL）才算失败
static EspAtResult_t EspAT_Command(const char *command,
                                   const char *expected,
                                   uint32_t timeout_ms)
{
    uint8_t attempt;
    uint8_t result;
    uint16_t length;

    if ((command == NULL) || (expected == NULL) ||
        (usart1MutexHandle == NULL))
    {
        return ESP_AT_RESULT_ERROR;
    }

    if (osMutexAcquire(usart1MutexHandle, timeout_ms) != osOK)
    {
        return ESP_AT_RESULT_BUSY;
    }

    length = (uint16_t)strlen(command);
    if ((length == 0U) || (length >= ESP_COMMAND_BUFFER_SIZE))
    {
        EspAT_RecordFail(expected);
        osMutexRelease(usart1MutexHandle);
        return ESP_AT_RESULT_ERROR;
    }

    for (attempt = 0U; attempt < ESP_AT_BUSY_RETRIES; attempt++)
    {
        EspAT_ClearRx();
        if (HAL_UART_Transmit(&huart1, (uint8_t *)command, length,
                              timeout_ms) != HAL_OK)
        {
            EspAT_RecordFail(expected);
            osMutexRelease(usart1MutexHandle);
            return ESP_AT_RESULT_ERROR;
        }

        result = EspAT_WaitText(expected, timeout_ms);
        if (result == 1U)
        {
            osMutexRelease(usart1MutexHandle);
            return ESP_AT_RESULT_OK;
        }
        if (result == 2U)
        {
            /* busy：命令被丢弃，等 ESP 空闲再重发同一条 */
            osDelay(ESP_AT_BUSY_RETRY_DELAY_MS);
            continue;
        }
        break; /* 0=超时/ERROR/FAIL：不再重发 */
    }

    EspAT_RecordFail(expected);
    osMutexRelease(usart1MutexHandle);
    return ESP_AT_RESULT_TIMEOUT;
}

/* 写入 MQTT 长度前缀字符串：2 字节大端长度 + 内容 */
static uint16_t EspAT_PutString(uint8_t *buffer, uint16_t index,
                                const char *text)
{
    uint16_t length = (uint16_t)strlen(text);
    buffer[index++] = (uint8_t)(length >> 8U);
    buffer[index++] = (uint8_t)(length & 0xFFU);
    memcpy(&buffer[index], text, length);
    return (uint16_t)(index + length);
}

/* MQTT 剩余长度变长编码（payload 都很小，通常只编出 1 字节） */
static uint16_t EspAT_EncodeLength(uint8_t *buffer, uint16_t length)
{
    uint16_t index = 0U;
    do
    {
        uint8_t digit = (uint8_t)(length % 128U);
        length = (uint16_t)(length / 128U);
        if (length != 0U)
        {
            digit |= 0x80U;
        }
        buffer[index++] = digit;
    } while (length != 0U);
    return index;
}

/* 组 MQTT CONNECT 报文，返回总长度（写入静态 esp_packet） */
static uint16_t EspAT_BuildConnect(void)
{
    uint16_t variable_length;
    uint16_t index = 0U;
    uint8_t flags = 0x02U; /* clean session = 1 */

    /* 先按各部分长度累加 remaining length，再回填编码 */
    variable_length = 0U;
    variable_length += 2U + 4U;                     /* "MQTT" 协议名 */
    variable_length += 1U + 1U + 2U;                /* level/flags/keepalive */
    variable_length += (uint16_t)(2U + strlen(ESP_MQTT_CLIENT_ID));
    if (ESP_MQTT_USERNAME[0] != '\0')
    {
        flags |= 0x80U;
        variable_length += (uint16_t)(2U + strlen(ESP_MQTT_USERNAME));
    }
    if (ESP_MQTT_PASSWORD[0] != '\0')
    {
        flags |= 0x40U;
        variable_length += (uint16_t)(2U + strlen(ESP_MQTT_PASSWORD));
    }

    esp_packet[index++] = 0x10U;                    /* CONNECT 类型 */
    index = (uint16_t)(index + EspAT_EncodeLength(&esp_packet[index],
                                                   variable_length));
    esp_packet[index++] = 0x00U;                    /* 协议名长度 0x0004 */
    esp_packet[index++] = 0x04U;
    memcpy(&esp_packet[index], "MQTT", 4U);         /* 协议名 */
    index = (uint16_t)(index + 4U);
    esp_packet[index++] = 0x04U;                    /* 协议级别 4 */
    esp_packet[index++] = flags;                    /* 连接标志 */
    esp_packet[index++] = 0x00U;                    /* keepalive 60s */
    esp_packet[index++] = 0x3CU;
    index = EspAT_PutString(esp_packet, index, ESP_MQTT_CLIENT_ID);
    if (ESP_MQTT_USERNAME[0] != '\0')
    {
        index = EspAT_PutString(esp_packet, index, ESP_MQTT_USERNAME);
    }
    if (ESP_MQTT_PASSWORD[0] != '\0')
    {
        index = EspAT_PutString(esp_packet, index, ESP_MQTT_PASSWORD);
    }
    return index;
}

/* 在透传收到的裸字节里找 CONNACK：0x20 0x02 <flags> <return code>。
 * 用最近 4 字节滑窗匹配，返回码为 0 才算连接成功。 */
static uint8_t EspAT_WaitConnack(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint8_t window[4];
    uint8_t count = 0U;
    uint8_t byte;

    while ((HAL_GetTick() - start) < timeout_ms)
    {
        while (EspAT_ReadByte(&byte) != 0U)
        {
            if (count < sizeof(window))
            {
                window[count++] = byte;
            }
            else
            {
                memmove(window, &window[1], sizeof(window) - 1U);
                window[sizeof(window) - 1U] = byte;
            }

            if ((count == sizeof(window)) &&
                (window[0] == 0x20U) && (window[1] == 0x02U))
            {
                return (window[3] == 0U) ? 1U : 0U;
            }
        }
        osDelay(1U);
    }
    return 0U;
}

void EspAT_Init(void)
{
    uint16_t packet_length;

    esp_state = ESP_NET_WIFI_CONNECTING;//状态归零
    esp_initialized = 0U;

     EspAT_ClearRx();

    if (HAL_UART_Receive_IT(&huart1, (uint8_t *)&esp_rx_byte, 1U) == HAL_ERROR)
    {
        EspAT_RecordFail("UART RX");
        esp_state = ESP_NET_OFFLINE;
        return;
    }
     (void)EspAT_Command(" AT+RESTORE\r\n", "OK", ESP_RESPONSE_TIMEOUT_MS);  //擦除FLASH参数

    //(void)EspAT_Command("AT+CWQAP\r\n", "OK", ESP_RESPONSE_TIMEOUT_MS);
    /* 基本握手；关回显、关自动连接失败也不影响主流程 */
    if (EspAT_Command("AT\r\n", "OK", ESP_RESPONSE_TIMEOUT_MS) != ESP_AT_RESULT_OK)
    {
        esp_state = ESP_NET_OFFLINE;
        return;
    }
    (void)EspAT_Command("ATE0\r\n", "OK", ESP_RESPONSE_TIMEOUT_MS);

    //(void)EspAT_Command("AT+CWRECONNCFG=0,0\r\n", "OK", ESP_RESPONSE_TIMEOUT_MS); //被动断开连接后不自动重连
    (void)EspAT_Command("AT+CWAUTOCONN=0\r\n", "OK", ESP_RESPONSE_TIMEOUT_MS);//似乎没用，每次ESP上电都会自动连接WIFI

    if (EspAT_Command("AT+CWMODE=1\r\n", "OK", ESP_RESPONSE_TIMEOUT_MS) != ESP_AT_RESULT_OK)
    {
        esp_state = ESP_NET_OFFLINE;
        return;
    }

    /* Wi-Fi：模块可能已被外部先上电并按存储配置自动连上目标 AP，也可能没连。
       顺序很关键——先 AT+CWJAP? 查当前关联状态：
       - 已连着目标 SSID：直接视为就绪，跳过重新入网。若仍无条件发 CWJAP，
         等于对一个"已连好的网"强制断开重连；重连窗口内再查 CWJAP? 又查不到，
         整条初始化就会反复失败 → 一直 OFFLINE（典型现象：ESP 先上电自动连上，
         程序后启动就卡死）。
       - 没连 / 连的是别的 AP：才发 CWJAP 入网并等 WIFI GOT IP，不能只看 OK。 */
    if ((strlen(ESP_WIFI_SSID) + strlen(ESP_WIFI_PASSWORD) + 16U) >=
        sizeof(esp_command))
    {
        esp_state = ESP_NET_OFFLINE;
        return;
    }

    (void)sprintf(esp_expect, "+CWJAP:\"%s\"", ESP_WIFI_SSID);

    if (EspAT_Command("AT+CWJAP?\r\n", esp_expect,
                      ESP_RESPONSE_TIMEOUT_MS) != ESP_AT_RESULT_OK)
    {
        /* 当前没连目标 AP：正常入网 */
        (void)sprintf(esp_command, "AT+CWJAP_CUR=\"%s\",\"%s\"\r\n", //AT+CWJAP_CUR=不写FLASH，防止每次程序运行前，ESP上电自动连接WIFI
                      ESP_WIFI_SSID, ESP_WIFI_PASSWORD);
        if (EspAT_Command(esp_command, "WIFI GOT IP",
                          ESP_WIFI_TIMEOUT_MS) != ESP_AT_RESULT_OK)
        {
            esp_state = ESP_NET_OFFLINE;
            return;
        }
    }
    esp_state = ESP_NET_WIFI_READY;

    /* Wi-Fi 刚关联完内部还在收尾，立刻发 TCP 会撞上 busy p...；停一小段再往下走 */
    osDelay(300U);


    /* TCP：必须等到 CONNECT（CLOSED/ERROR 都会失败） */
    esp_state = ESP_NET_TCP_CONNECTING;
    (void)sprintf(esp_command, "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n",
                  ESP_MQTT_HOST, (unsigned)ESP_MQTT_PORT);
    if (EspAT_Command(esp_command, "CONNECT", ESP_TCP_TIMEOUT_MS) != ESP_AT_RESULT_OK)
    {
        esp_state = ESP_NET_OFFLINE;
        return;
    }

    /* 进入透传：CIPMODE=1 后 CIPSEND 给出 '>'，之后串口数据直达 TCP */
    if ((EspAT_Command("AT+CIPMODE=1\r\n", "OK", ESP_RESPONSE_TIMEOUT_MS) != ESP_AT_RESULT_OK) ||
        (EspAT_Command("AT+CIPSEND\r\n", ">", ESP_RESPONSE_TIMEOUT_MS) != ESP_AT_RESULT_OK))
    {
        esp_state = ESP_NET_OFFLINE;
        return;
    }

    /* 透传下发 MQTT CONNECT，必须收到真实 CONNACK */
    esp_state = ESP_NET_MQTT_CONNECTING;
    packet_length = EspAT_BuildConnect();
    if ((usart1MutexHandle == NULL) ||
        (osMutexAcquire(usart1MutexHandle, ESP_RESPONSE_TIMEOUT_MS) != osOK) ||
        (HAL_UART_Transmit(&huart1, esp_packet, packet_length,
                           ESP_RESPONSE_TIMEOUT_MS) != HAL_OK) ||
        (EspAT_WaitConnack(ESP_TCP_TIMEOUT_MS) == 0U))
    {
        if (usart1MutexHandle != NULL)
        {
            (void)osMutexRelease(usart1MutexHandle);
        }
        EspAT_RecordFail("MQTT CONNACK");
        esp_state = ESP_NET_OFFLINE;
        return;
    }
    (void)osMutexRelease(usart1MutexHandle);

    esp_state = ESP_NET_MQTT_READY;
    esp_initialized = 1U;
    esp_last_ping = HAL_GetTick();
}

// 按照ESP_PING_PERIOD_MS 发 MQTT PINGREQ 等不到 PINGRESP 判掉线 
void EspAT_Update(void)
{
    static const uint8_t ping[2] = {0xC0U, 0x00U}; /* MQTT PINGREQ */
    uint32_t start;
    uint8_t got_pingresp = 0U;
    uint8_t byte;

    if (esp_state != ESP_NET_MQTT_READY)
    {
        return;
    }
    if ((HAL_GetTick() - esp_last_ping) < ESP_PING_PERIOD_MS)
    {
        return;
    }
    if (usart1MutexHandle == NULL)
    {
        esp_state = ESP_NET_OFFLINE;
        return;
    }

    /* 串口正被发布占用就跳过本次保活，避免误判掉线 */
    if (osMutexAcquire(usart1MutexHandle, 100U) != osOK)
    {
        return;
    }
    if (HAL_UART_Transmit(&huart1, (uint8_t *)ping, sizeof(ping),
                          500U) != HAL_OK)
    {
        (void)osMutexRelease(usart1MutexHandle);
        esp_state = ESP_NET_OFFLINE;
        return;
    }

    /* 透传下 PINGRESP 是裸字节 0xD0 0x00 */
    start = HAL_GetTick();
    while ((HAL_GetTick() - start) < ESP_RESPONSE_TIMEOUT_MS)
    {
        if (EspAT_ReadByte(&byte) != 0U)
        {
            if (byte == 0xD0U)
            {
                got_pingresp = 1U;
                break;
            }
        }
        osDelay(1U);
    }
    (void)osMutexRelease(usart1MutexHandle);
    esp_last_ping = HAL_GetTick();
    if (got_pingresp == 0U)
    {
        esp_state = ESP_NET_OFFLINE;
    }
}

/* 排障 getter：最近失败阶段与响应（供 UI 显示） */
const char *EspAT_GetLastStage(void)
{
    return esp_last_stage;
}

const char *EspAT_GetLastResponse(void)
{
    return esp_last_response;
}

EspNetState_t EspAT_GetNetworkState(void)
{
    return esp_state;
}

uint8_t EspAT_IsReady(void)
{
    return (esp_initialized != 0U) && (esp_state == ESP_NET_MQTT_READY);
}

EspAtResult_t EspAT_PublishJson(const char *json, uint16_t length)
{
    uint16_t topic_length;
    uint16_t remaining_length;
    uint16_t index;

    if ((json == NULL) || (length == 0U) ||
        (EspAT_IsReady() == 0U))
    {
        return ESP_AT_RESULT_ERROR;
    }

    topic_length = (uint16_t)strlen(ESP_MQTT_TOPIC);
    remaining_length = (uint16_t)(2U + topic_length + length);
    if ((remaining_length + 5U) > sizeof(esp_packet))
    {
        return ESP_AT_RESULT_ERROR;
    }

    index = 0U;
    esp_packet[index++] = 0x30U; /* MQTT PUBLISH, QoS0 */
    index = (uint16_t)(index + EspAT_EncodeLength(&esp_packet[index],
                                                   remaining_length));
    index = EspAT_PutString(esp_packet, index, ESP_MQTT_TOPIC);
    memcpy(&esp_packet[index], json, length);
    index = (uint16_t)(index + length);

    if (usart1MutexHandle == NULL)
    {
        return ESP_AT_RESULT_ERROR;
    }
    if (osMutexAcquire(usart1MutexHandle, ESP_RESPONSE_TIMEOUT_MS) != osOK)
    {
        return ESP_AT_RESULT_BUSY;
    }
    if (HAL_UART_Transmit(&huart1, esp_packet, index,
                          ESP_RESPONSE_TIMEOUT_MS) != HAL_OK)
    {
        (void)osMutexRelease(usart1MutexHandle);
        esp_state = ESP_NET_OFFLINE;
        return ESP_AT_RESULT_ERROR;
    }
    (void)osMutexRelease(usart1MutexHandle);
    return ESP_AT_RESULT_OK;
}

EspAtResult_t EspAT_PublishData(float temperature, float humidity)
{
    int temperature10;
    int humidity10;
    int length;

    /* 使用整数十分位，避免依赖 ARMCC 的浮点 printf。 */
    temperature10 = (int)(temperature * 10.0f +
                          ((temperature >= 0.0f) ? 0.5f : -0.5f));
    humidity10 = (int)(humidity * 10.0f + 0.5f);
    length = sprintf(esp_json,
                     "{\"type\":\"realtime_data\","
                     "\"temperature\":%d.%d,"
                     "\"humidity\":%d.%d}",
                     temperature10 / 10, (temperature10 < 0) ?
                     -(temperature10 % 10) : (temperature10 % 10),
                     humidity10 / 10, humidity10 % 10);
    if ((length <= 0) || ((uint32_t)length >= sizeof(esp_json)))
    {
        return ESP_AT_RESULT_ERROR;
    }
    return EspAT_PublishJson(esp_json, (uint16_t)length);
}

/* ISR：只搬字节进环形缓冲并重启接收，不做任何字符串解析 */
extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if ((huart != NULL) && (huart->Instance == USART1))
    {
        uint16_t next = (uint16_t)((esp_rx_head + 1U) % ESP_RX_BUFFER_SIZE);
        if (next != esp_rx_tail)
        {
            esp_rx_buffer[esp_rx_head] = esp_rx_byte;
            esp_rx_head = next;
        }
        (void)HAL_UART_Receive_IT(&huart1, (uint8_t *)&esp_rx_byte, 1U);
    }
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if ((huart != NULL) && (huart->Instance == USART1))
    {
        (void)HAL_UART_Receive_IT(&huart1, (uint8_t *)&esp_rx_byte, 1U);
    }
}
