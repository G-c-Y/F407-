/* ----------------------------------------------------------------------------
 * LVGL 输入设备移植层（触摸屏）
 * ----------------------------------------------------------------------------
 * 职责：把触摸驱动的坐标/按下状态喂给 LVGL（LV_INDEV_TYPE_POINTER）。
 * 设计要点：
 *  - touchpad_read 由 LVGL 在 UI 任务中周期调用（LV_INDEV_DEF_READ_PERIOD），
 *    不在中断里采样，避免占用 ISR。
 *  - TP_Get_Calibrated 已把原始 ADC 换算成屏幕逻辑坐标（含换轴/镜像校准，
 *    校准参数在 dvc_lcd_touch.cpp 内），这里只做边界钳制。
 *  - LCD_W/LCD_H 必须与当前屏幕分辨率一致（竖屏 240x320）。
 * -------------------------------------------------------------------------- */
#include "lv_port_indev.h"
#include "dvc_lcd_touch.h"
#include <stdbool.h>

lv_indev_t *indev_touchpad;

static void touchpad_read(lv_indev_drv_t *indev_drv,
                          lv_indev_data_t *data)
{
    static uint16_t last_x;   /* 松开时沿用上一次坐标，避免 UI 元素跳动 */
    static uint16_t last_y;
    uint16_t x;
    uint16_t y;

    (void)indev_drv;
    if (TP_Get_Calibrated(&x, &y) != 0U)
    {
        if (x >= LCD_W) x = LCD_W - 1U;
        if (y >= LCD_H) y = LCD_H - 1U;
        last_x = x;
        last_y = y;
        data->point.x = (lv_coord_t)x;
        data->point.y = (lv_coord_t)y;
        data->state = LV_INDEV_STATE_PR;
    }
    else
    {
        data->point.x = (lv_coord_t)last_x;
        data->point.y = (lv_coord_t)last_y;
        data->state = LV_INDEV_STATE_REL;
    }
}

void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
}
