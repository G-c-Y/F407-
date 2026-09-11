# STM32F407ZGT6 工业管理在线监控系统

## 项目简介
基于STM32F407ZGT6微控制器开发的工业管理在线监控系统，集成LVGL图形界面、FreeRTOS实时操作系统、Modbus通信协议和MQTT物联网协议。

## 主要特性
- **MCU**: STM32F407ZGT6 (168MHz主频，1MB Flash，192KB RAM)
- **操作系统**: FreeRTOS实时操作系统
- **图形界面**: LVGL图形库，支持触摸屏
- **通信协议**: 
  - Modbus RTU/TCP协议
  - MQTT物联网协议
  - ESP8266 WiFi模块接口
- **外设支持**:
  - FSMC LCD接口
  - SPI接口 (W25Q128 Flash存储器)
  - UART接口
  - GPIO控制 (LED、按键)
  - 独立看门狗(IWDG)

## 目录结构
- **APP**: 应用层代码，包括AT指令处理、Modbus、MQTT、UI界面等
- **BSP**: 板级支持包，LED、按键等底层驱动
- **Core**: 核心代码，包括主程序、中断处理、系统配置
- **Drivers**: STM32 HAL驱动库
- **DVC**: 设备层代码，LCD驱动、触摸屏、ESP8266、Flash等
- **LVGL**: 图形库源码
- **Middlewares**: 中间件，包括FreeRTOS
- **Service**: 服务层代码，缓存、数据管理、事件处理等
- **User_Task**: 用户任务管理

## 开发环境
- IDE: Keil MDK-ARM
- MCU: STM32F407ZGT6
- 编译工具: ARMCC

## 系统架构
项目采用分层架构设计：
1. **应用层(APP)**: 业务逻辑处理
2. **服务层(Service)**: 数据处理和事件管理
3. **设备层(DVC)**: 外设驱动和通信模块
4. **板级支持层(BSP)**: 硬件抽象层
5. **核心层(Core)**: 系统核心功能

## 主要功能模块
- **LCD显示**: 支持TFT LCD显示屏，触摸屏输入
- **网络通信**: 通过ESP8266实现WiFi连接和MQTT通信
- **数据存储**: W25Q128 Flash存储器用于数据持久化
- **Modbus通信**: 支持Modbus RTU/TCP协议，用于工业设备通信
- **实时任务**: FreeRTOS多任务调度，实现系统实时性

## 硬件连接
- LCD: FSMC接口
- Touch: SPI接口
- Flash: SPI接口
- WiFi: UART接口 (ESP8266)
- Key/PWM: GPIO接口

## 许可证
MIT License

Co-Authored-By: Claude Code <noreply@anthropic.com>