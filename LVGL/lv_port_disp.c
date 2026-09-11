/* ----------------------------------------------------------------------------
 * LVGL 显示移植层（FSMC 16 位并口 LCD）
 * ----------------------------------------------------------------------------
 * 职责：把 LVGL 渲染好的局部区域（flush 回调）通过现有 dvc_lcd 底层刷到屏幕。
 * 设计要点：
 *  - 只用一块部分缓冲区（draw_buffer），LVGL 会分块渲染（partial flush），
 *    比整屏双缓冲更省 RAM；行数 LV_PORT_DRAW_LINES 越大渲染越快、RAM 越高。
 *  - flush 用 lcd_set_window + 连续 GRAM 写入批量刷，不逐像素调用画点。
 *  - 颜色为 RGB565，本工程 16 位并口不交换字节（LV_COLOR_16_SWAP=0），
 *    因此 color_p->full 可直接作为 16 位像素写给 LCD。
 *  - 本文件所有函数都运行在 UI 任务上下文（lv_timer_handler 内），
 *    其它任务不得调用，避免多任务同时写屏。
 * -------------------------------------------------------------------------- */
#include "lv_port_disp.h"
#include "dvc_lcd.h"
#include <stdbool.h>

#define LV_PORT_DRAW_LINES  24U   /* 一次刷新的最大行数（部分缓冲高度） */

static lv_disp_draw_buf_t draw_buf;                       /* LVGL 显示缓冲描述 */
static lv_color_t draw_buffer[240U * LV_PORT_DRAW_LINES]; /* 实际像素缓冲：竖屏宽240 */
static bool flush_enabled = true;                         /* 息屏/清屏时关刷新 */

static void lv_port_flush(lv_disp_drv_t *disp_drv,
                          const lv_area_t *area,
                          lv_color_t *color_p)
{
    uint16_t width;
    uint16_t height;
    uint32_t pixel_count;

    if ((flush_enabled != false) &&
        (area->x1 >= 0) && (area->y1 >= 0) &&
        ((uint16_t)area->x2 < lcddev.width) &&
        ((uint16_t)area->y2 < lcddev.height))
    {
        width = (uint16_t)(area->x2 - area->x1 + 1);
        height = (uint16_t)(area->y2 - area->y1 + 1);
        pixel_count = (uint32_t)width * height;
        lcd_set_window((uint16_t)area->x1, (uint16_t)area->y1,
                       width, height);
        lcd_write_ram_prepare();
        while (pixel_count-- != 0U)
        {
            lcd_wr_data((uint16_t)color_p->full);
            color_p++;
        }
    }
    lv_disp_flush_ready(disp_drv);
}

void lv_port_disp_init(void)
{
    static lv_disp_drv_t disp_drv;

    lv_disp_draw_buf_init(&draw_buf, draw_buffer, NULL,
                          sizeof(draw_buffer) / sizeof(draw_buffer[0]));
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = lcddev.width;
    disp_drv.ver_res = lcddev.height;
    disp_drv.flush_cb = lv_port_flush;
    disp_drv.draw_buf = &draw_buf;
    (void)lv_disp_drv_register(&disp_drv);
}

void disp_enable_update(void)
{
    flush_enabled = true;
}

void disp_disable_update(void)
{
    flush_enabled = false;
}
