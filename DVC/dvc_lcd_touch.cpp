#include "dvc_lcd_touch.h"
//#include "dvc_lcd.h"
#include "stdio.h"
#include "stdlib.h"
uint32_t raw_x_see = 0;
uint32_t raw_y_see = 0;


#define TP_DLY()  { for (volatile int _i = 0; _i < 10; _i++); }   /* ≈1us @168MHz */

  /* 坐标映射边界：先用占位值，烧录后用串口读四角实际 AD 值再填 */
  #define TX_MIN   0x00000756
  #define TX_MAX   0x0000004E
  #define TY_MIN   0x000007B5
  #define TY_MAX   0x000000514

  /* ========== 软件 SPI：写 1 字节（高位在前） ========== */
  static void TP_WriteByte(uint8_t data)
  {
      for (uint8_t i = 0; i < 8; i++)
      {
          if (data & 0x80)  T_MOSI_SET;
          else              T_MOSI_CLR;
          data <<= 1;
          T_SCK_CLR;  TP_DLY();
          T_SCK_SET;  TP_DLY();      /* 上升沿锁存 */
      }
  }

  /* ========== 软件 SPI：发命令 + 读 12 位 AD ========== */
  static uint16_t TP_ReadAD(uint8_t cmd)
  {
      uint16_t num = 0;
      T_SCK_CLR;
      T_MOSI_CLR;
      T_CS_CLR;                      /* 选中 XPT2046 */
      TP_WriteByte(cmd);
      TP_DLY();                      /* 等待转换完成 */
      for (uint8_t i = 0; i < 16; i++)   /* 读 16 位，高 12 位有效 */
      {
          num <<= 1;
          T_SCK_CLR;  TP_DLY();
          T_SCK_SET;
          if (T_MISO_GET)  num++;
      }
      T_CS_SET;
      return num >> 4;
  }

  /* ========== 5 次采样，排序去极值取平均（照搬正点原子） ========== */
  static uint16_t TP_ReadXOY(uint8_t cmd)
  {
      uint16_t buf[5], temp, i, j;
      for (i = 0; i < 5; i++)  buf[i] = TP_ReadAD(cmd);
      for (i = 0; i < 4; i++)
          for (j = i + 1; j < 5; j++)
              if (buf[i] > buf[j])  { temp = buf[i]; buf[i] = buf[j]; buf[j] = temp; }
      return (buf[1] + buf[2] + buf[3]) / 3;   /* 去掉最大和最小后取平均 */
  }

  void TP_Init(void)
  {
      T_CS_SET;                  /* CS 空闲为高（不选中） */
      TP_ReadXOY(0xD0);          /* 首次读取，让芯片稳定 */
  }

  uint8_t TP_Get_Calibrated(uint16_t *x, uint16_t *y)
  {
      /* 判按下：读压力 Z1/Z2，阈值按实测微调 */
      uint16_t z1 = TP_ReadAD(0xB1);
      uint16_t z2 = TP_ReadAD(0xC1);
      if (z1 < 50 || z2 > 3000)  return 0;

      uint16_t raw_x = TP_ReadXOY(0xD0);   /* X 轴 AD */
      uint16_t raw_y = TP_ReadXOY(0x90);   /* Y 轴 AD */
      
      raw_x_see = raw_x;
      raw_y_see = raw_y;

      /* 换轴 + 归一化（映射边界用实测四角值） */
      int16_t lcd_y = (raw_y - TY_MIN) * (LCD_W - 1) / (TY_MAX - TY_MIN);
      int16_t lcd_x = (raw_x - TX_MIN) * (LCD_H - 1) / (TX_MAX - TX_MIN);

      lcd_x = (LCD_W - 1) - lcd_x;     /* 镜像修正，方向反了再删这两行 */
      lcd_y = (LCD_H - 1) - lcd_y;

      if (lcd_x < 0)  lcd_x = 0;
      if (lcd_x >= LCD_W)  lcd_x = LCD_W - 1;
      if (lcd_y < 0)  lcd_y = 0;
      if (lcd_y >= LCD_H)  lcd_y = LCD_H - 1;

      *x = lcd_x;
      *y = lcd_y;
      return 1;
  }









