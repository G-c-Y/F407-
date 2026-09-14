#ifndef DVC_W25Q128_H
#define DVC_W25Q128_H

#include "stm32f4xx_hal.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
   #endif

#define  sFLASH_ID                        0XEF4018   // W25Q128
#define SPI_FLASH_PageSize              256
#define SPI_FLASH_PerWritePageSize      256

/* 命令定义 */
#define W25X_WriteEnable        0x06 
#define W25X_WriteDisable        0x04 
#define W25X_ReadStatusReg        0x05 
#define W25X_WriteStatusReg        0x01 
#define W25X_ReadData            0x03 
#define W25X_FastReadData        0x0B 
#define W25X_FastReadDual        0x3B 
#define W25X_PageProgram        0x02 
#define W25X_BlockErase            0xD8 
#define W25X_SectorErase        0x20 
#define W25X_ChipErase            0xC7 
#define W25X_PowerDown            0xB9 
#define W25X_ReleasePowerDown   0xAB 
#define W25X_DeviceID           0xAB 
#define W25X_ManufactDeviceID   0x90 
#define W25X_JedecDeviceID      0x9F

#define WIP_Flag                0x01
#define Dummy_Byte              0xFF

#define SPIx                             SPI1

#define SpiHandle                         hspi1

#define FLASH_CS_PIN                     GPIO_PIN_14        // PB14
#define FLASH_CS_GPIO_PORT               GPIOB

/* CS 控制（使用 HAL 标准函数）*/
#define SPI_FLASH_CS_LOW()       HAL_GPIO_WritePin(FLASH_CS_GPIO_PORT, FLASH_CS_PIN, GPIO_PIN_RESET)
#define SPI_FLASH_CS_HIGH()      HAL_GPIO_WritePin(FLASH_CS_GPIO_PORT, FLASH_CS_PIN, GPIO_PIN_SET)

/* 超时 */
#define SPIT_FLAG_TIMEOUT         ((uint32_t)0x1000)
#define SPIT_LONG_TIMEOUT         ((uint32_t)(10 * SPIT_FLAG_TIMEOUT))

/* 调试输出 */
#define FLASH_DEBUG_ON         1
#define FLASH_INFO(fmt,arg...)          // printf("<<-FLASH-INFO->> "fmt"\n",##arg)
#define FLASH_ERROR(fmt,arg...)         // printf("<<-FLASH-ERROR->> "fmt"\n",##arg)
// #define FLASH_DEBUG(fmt,arg...)          do{\
//                                           if(FLASH_DEBUG_ON)\
//                                           printf("<<-FLASH-DEBUG->> [%d]"fmt"\n",__LINE__, ##arg);\
//                                           }while(0)

/* 函数声明 */
void SPI_FLASH_Init(void);
void SPI_FLASH_SectorErase(uint32_t SectorAddr);
void SPI_FLASH_BulkErase(void);
void SPI_FLASH_PageWrite(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
void SPI_FLASH_BufferWrite(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
void SPI_FLASH_BufferRead(uint8_t* pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead);
uint32_t SPI_FLASH_ReadID(void);
uint32_t SPI_FL_ReadDeviceID(void);
void SPI_FLASH_StartReadSequence(uint32_t ReadAddr);
void SPI_Flash_PowerDown(void);
void SPI_Flash_WAKEUP(void);
uint8_t SPI_FLASH_ReadByte(void);
uint8_t SPI_FLASH_SendByte(uint8_t byte);
uint16_t SPI_FLASH_SendHalfWord(uint16_t HalfWord);
void SPI_FLASH_WriteEnable(void);
void SPI_FLASH_WaitForWriteEnd(void);


#ifdef __cplusplus
}
#endif

#endif

