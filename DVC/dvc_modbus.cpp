#include "dvc_modbus.h"

//Modbus RTU CRC16校验函数（标准RTU协议的CRC-16-MODBUS多项式）
//参数：data - 要校验的数据缓冲区，length - 数据长度
//返回：计算得到的CRC16校验值
uint16_t Modbus_CRC16(uint8_t *data , uint8_t length)
{
    uint16_t crc=0xffff;  //CRC初始值（0xFFFF）
    if (data == NULL)
    {
        return 0U;  //参数检查，返回0表示错误
    }

	while(length--)
		{
			crc^=*data++;  //当前字节与CRC进行异或
			for(int i=0;i<8;i++)  //逐位处理
           {
				if(crc &1)  //如果最低位为1
				{
					crc=(crc>>1)^0xA001;  //右移1位并异以多项式0xA001
				}
				else
				{
					crc>>=1;  //直接右移1位
				}
			}
		}
		return crc;  //返回最终的CRC校验值
}

//等待UART发送完成
//参数：timeout_ms - 超时时间（毫秒）
//返回：1=发送完成，0=超时
static uint8_t Modbus_WaitTransmitComplete(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();  //开始等待的时间戳

    //等待发送完成标志（TC=Transmission Complete）
    while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) == RESET)
    {
        //检查是否超时
        if ((HAL_GetTick() - start) >= timeout_ms)
        {
            return 0U;  //超时
        }
    }

    return 1U;  //发送完成
}
//读取Modbus传感器数据（主函数）
//参数：data - 指向ModbusData_t结构体的指针，用于存储读取的数据
//返回：1=成功，0=失败
uint8_t Modbus_readDev(ModbusData_t *data)
{
    //参数检查
    if (data == NULL)
    {
        return 0U;
    }

    //每次开始新的读取前，先清除上一次可能遗留的有效标志
    data->temperture_valid = 0.0f;
    data->humidity_valid = 0.0f;

    //寄存器数组：用于存储从Modbus设备读取的两个寄存器值
    uint16_t registers[MODBUS_REGISTER_COUNT];
    //请求帧：构建Modbus RTU读取请求
    uint8_t request[MODBUS_REQUEST_LENGTH];
    //响应帧：存储Modbus设备的响应数据
    uint8_t response[MODBUS_RESPONSE_LENGTH];
    //CRC校验值：用于请求和响应的完整性检查
    uint16_t crc;

    //构建Modbus请求帧
    request[0] = MODBUS_SLAVE_ADDRESS;      //从站地址
    request[1] = MODBUS_FUNCTION_READ;      //功能码（03H=读取保持寄存器）
    request[2] = (uint8_t)(MODBUS_START_REGISTER >> 8U);  //起始寄存器地址高字节
    request[3] = (uint8_t)(MODBUS_START_REGISTER & 0xFFU); //起始寄存器地址低字节
    request[4] = 0x00U;                     //读取寄存器数量高字节（固定为0）
    request[5] = MODBUS_REGISTER_COUNT;     //读取寄存器数量低字节（2个）

    //计算请求帧的CRC校验值（对前6个字节进行校验）
    crc = Modbus_CRC16(request, 6U);
    //将CRC校验值附加到请求帧末尾（低字节在前，高字节在后）
    request[6] = (uint8_t)(crc & 0xFFU);    //CRC低字节
    request[7] = (uint8_t)(crc >> 8U);     //CRC高字节

    //清空响应缓冲区
    memset(response, 0, sizeof(response));

    /* 清除UART溢出错误标志（ORE）和丢弃上一帧的残留字节 */
    //先读SR（状态寄存器）再读DR（数据寄存器），可清除ORE标志
    __HAL_UART_CLEAR_OREFLAG(&huart2);

    //清除接收缓冲区中的残留数据
    //检查接收寄存器是否非空（RXNE=Receive Not Empty）
    while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET)
    {
        //读取并丢弃残留字节（避免影响本次通信）
        volatile uint8_t discarded = (uint8_t)(huart2.Instance->DR & 0xFFU);
        (void)discarded;  //避免编译器警告
    }
    //启用RS485发送模式
    MODBUS_TX_ENABLE();

    //发送请求帧到Modbus设备
    if (HAL_UART_Transmit(&huart2,
                          request,
                          sizeof(request),
                          MODBUS_UART_TIMEOUT_MS) != HAL_OK)
    {
        //发送失败，切换回接收模式并返回失败
        MODBUS_RX_ENABLE();
        return 0U;
    }

    //等待发送完成
    if (Modbus_WaitTransmitComplete(MODBUS_TC_TIMEOUT_MS) == 0U)
    {
        //发送超时，切换回接收模式并返回失败
        MODBUS_RX_ENABLE();
        return 0U;
    }

    //发送完成，切换回接收模式准备接收响应
    MODBUS_RX_ENABLE();

    //接收响应帧
    if (HAL_UART_Receive(&huart2,
                         response,
                         sizeof(response),
                         MODBUS_UART_TIMEOUT_MS) != HAL_OK)
    {
        //接收超时，返回失败
        return 0U;
    }

    //验证响应帧的基本内容
    //检查从站地址
    if (response[0] != MODBUS_SLAVE_ADDRESS)
    {
        return 0U;
    }
    //检查功能码（必须与请求相同，除了最高位可能会有异常指示）
    if (response[1] != MODBUS_FUNCTION_READ)
    {
        return 0U;
    }
    //检查数据长度（每个寄存器2字节，MODBUS_REGISTER_COUNT个寄存器）
    if (response[2] != (uint8_t)(MODBUS_REGISTER_COUNT * 2U))
    {
        return 0U;
    }

    //验证响应帧的CRC校验值
    //对响应数据（除了最后2个字节的CRC）进行CRC校验
    crc = Modbus_CRC16(response, MODBUS_RESPONSE_LENGTH - 2U);
    //检查CRC低字节和高字节是否匹配
    if ((response[7] != (uint8_t)(crc & 0xFFU)) ||
        (response[8] != (uint8_t)(crc >> 8U)))
    {
        return 0U;  //CRC校验失败
    }

//解析响应数据，将字节转换为16位寄存器值
    //Modbus协议是大端序（高位在前，低位在后）
    for (uint8_t i = 0U; i < MODBUS_REGISTER_COUNT; i++)
    {
        registers[i] = (uint16_t)(((uint16_t)response[3U + i * 2U] << 8U) | //高字节
                                 (uint16_t)response[4U + i * 2U]);         //低字节
    }

    //转换数据格式
    //温度是有符号16位值（int16_t），乘以0.1转换为摄氏度
    data->temperature = (float)((int16_t)registers[0]) * 0.1f;
    //湿度是无符号值（uint16_t），乘以0.1转换为百分比RH
    data->humidity = (float)registers[1] * 0.1f;

    //数据有效性检查（根据传感器量程限制）
    //温度范围：-40°C ~ +125°C（典型Modbus温度传感器）
    //湿度范围：0% ~ 100%RH
    if ((data->temperature < -40.0f) ||
        (data->temperature > 125.0f) ||
        (data->humidity < 0.0f) ||
        (data->humidity > 100.0f))
    {
        return 0U;  //数据超出合理范围，视为无效
    }

    //数据有效
    data->temperture_valid = 1.0f;
    data->humidity_valid = 1.0f;
    return 1U;  //读取成功

}

//外部数据获取接口（提供统一的访问方式）
//参数：data - 指向ModbusData_t结构体的指针，用于存储读取的数据
//返回：1=成功，0=失败
uint8_t Modbus_dataGet(ModbusData_t *data){
    //参数检查
    if (data == NULL)
    {
        return 0U;
    }

    //清空数据结构（确保初始化）
    memset(data, 0, sizeof(*data));

    //调用内部读取函数获取数据
    if (Modbus_readDev(data) == 0U)
    {
        return 0U;  //读取失败
    }

    return 1U;  //读取成功
}


