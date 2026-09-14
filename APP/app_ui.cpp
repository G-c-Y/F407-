#include "app_ui.h"
#include "TaskHandle.h"
#include "svc_cache.h"
#include "app_ai.h"
#include "dvc_lcd.h"
#include "cmsis_os2.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include <stdio.h>

//LVGL页面句柄：所有更新只在UITASK内调用，避免跨线程触碰LVGL
static lv_obj_t *ui_label_state;    //网络状态标签
static lv_obj_t *ui_label_ai;       //AI 判断结果标签
static lv_obj_t *ui_label_temp;     //温度标签
static lv_obj_t *ui_label_humi;     //湿度标签
static lv_obj_t *ui_label_light;    //光照标签
static lv_obj_t *ui_label_cache;    //缓存计数标签
static uint32_t ui_last_tick;       //上次更新时间戳（用于防抖）

//创建一个带指定颜色/位置的文本标签
static lv_obj_t *App_UI_MakeLabel(lv_obj_t *parent, const char *text,
                                  uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);  //创建标签控件

    //设置文本颜色
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0U);
    //设置字体
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0U);
    //设置显示文本
    lv_label_set_text(label, text);
    return label;  //返回创建的标签对象
}

//创建一块圆角深色底板，用于承载数值，视觉分区更清晰
static lv_obj_t *App_UI_MakePanel(lv_obj_t *parent, uint32_t bg,
                                  lv_coord_t x, lv_coord_t y,
                                  lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *panel = lv_obj_create(parent);  //创建面板控件

    //设置背景完全不透明
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0U);
    //设置背景颜色
    lv_obj_set_style_bg_color(panel, lv_color_hex(bg), 0U);
    //设置边框宽度
    lv_obj_set_style_border_width(panel, 1U, 0U);
    //设置边框颜色（深灰色）
    lv_obj_set_style_border_color(panel, lv_color_hex(0x2A3644U), 0U);
    //设置圆角半径
    lv_obj_set_style_radius(panel, 6U, 0U);
    //设置阴影宽度为0（不显示阴影）
    lv_obj_set_style_shadow_width(panel, 0U, 0U);
    //设置面板位置
    lv_obj_set_pos(panel, x, y);
    //设置面板大小
    lv_obj_set_size(panel, w, h);
    return panel;  //返回创建的面板对象
}

//创建主页面布局（竖屏240x320）
void App_UI_CreateMainPage(void)
{
    lv_obj_t *scr = lv_scr_act();    //获取当前活动屏幕
    lv_obj_t *panel;                //临时面板对象
    lv_coord_t W = lcddev.width;    //屏幕宽度（竖屏通常240）
    lv_coord_t H = lcddev.height;   //屏幕高度（竖屏通常320）
    lv_coord_t x = 8;               //水平边距
    lv_coord_t cw;                  //内容宽度

    //如果LCD尺寸未初始化，使用默认竖屏尺寸
    if (W == 0U) W = 240;
    if (H == 0U) H = 320;
    cw = W - 2 * x + 2;  //计算内容宽度（减去左右边距）

    //设置深色全局背景，浅色文字保证对比度
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0B0E14U), 0U);  //深蓝色背景
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0U);              //完全不透明

    //状态条：左侧显示网络状态，右侧显示缓存计数
    panel = App_UI_MakePanel(scr, 0x141C26U, x, 12, cw, 46);    //深蓝色状态栏
    ui_label_state = App_UI_MakeLabel(panel, "OFFLINE", 0xFF8A50U);  //橙色状态文字
    lv_obj_align(ui_label_state, LV_ALIGN_LEFT_MID, 12, 0);     //左对齐
    ui_label_cache = App_UI_MakeLabel(panel, "CACHE:0", 0x7FD6FFU);  //青色缓存计数
    lv_obj_align(ui_label_cache, LV_ALIGN_RIGHT_MID, -12, 0);    //右对齐
    ui_label_ai = App_UI_MakeLabel(panel, "AI:--", 0x8A93A6U);   //灰色 AI 状态（未就绪）
    lv_obj_align(ui_label_ai, LV_ALIGN_CENTER, 0, 0);            //状态栏中部显示

    //温度显示区域
    panel = App_UI_MakePanel(scr, 0x141C26U, x, 66, cw, 76);    //深蓝色温度面板
    ui_label_temp = App_UI_MakeLabel(panel, "T:--.- C", 0x8FE3FFU);  //浅蓝色温度文字
    lv_obj_center(ui_label_temp);  //居中显示

    //湿度显示区域
    panel = App_UI_MakePanel(scr, 0x141C26U, x, 150, cw, 76);   //深蓝色湿度面板
    ui_label_humi = App_UI_MakeLabel(panel, "H: --.- %", 0xA8E6A1U);
    lv_obj_center(ui_label_humi);

    //光照显示区域
    panel = App_UI_MakePanel(scr, 0x141C26U, x, 234, cw, 76);    //深蓝色光照面板
    ui_label_light = App_UI_MakeLabel(panel, "L: --", 0xFFE08AU);  //黄色光照文字
    lv_obj_center(ui_label_light);  //居中显示

    //记录初始化时间戳
    ui_last_tick = HAL_GetTick();
}

//UI任务主函数，负责LVGL界面更新和数据显示
void App_UITask(void *argument)
{
    GatewayData_t data;  //网关数据结构
    uint32_t now;        //当前时间戳
    AiResult_t ai_res;   //AI 推理结果

    (void)argument;  //避免编译器警告

    //首次进入UI任务时才初始化LVGL（此时RTOS已运行、LCD已就绪）
    lv_init();           //初始化LVGL库
    lv_port_disp_init();  //初始化显示驱动（240x320竖屏）
    lv_port_indev_init(); //初始化输入设备（触摸屏）
    App_UI_CreateMainPage();  //创建主页面布局

    for (;;)
    {
        /* 给 LVGL 提供毫秒 tick（LV_TICK_CUSTOM=0 时需要手动喂） */
        now = HAL_GetTick();
        lv_tick_inc(now - ui_last_tick);
        ui_last_tick = now;

        if (osMessageQueueGet(lvglDataQueueHandle,
                              &data, NULL, 0U) == osOK)
        {
            App_UI_UpdateData(&data);
            App_AI_Feed(&data);   //喂入 AI 滑动窗口（与 UI 同任务，无需加锁）
        }

        App_UI_UpdateState(SvcState_Get());
        App_UI_UpdateCacheCount(SvcCache_Count());
        //每 2 秒触发一次推理（内部节流），产生新结果才刷新 AI 标签
        if (App_AI_Process(&ai_res) != 0U)
        {
            App_UI_UpdateAiState(ai_res.state, 1U);
        }

        lv_timer_handler(); /* LVGL 渲染与触摸采样都在本任务完成 */
        osDelay(5U);
    }
}

void App_UI_UpdateData(const GatewayData_t *data)
{
    int temperature10;
    int humidity10;
    int light;
    int t_int, t_frac, h_int, h_frac;

    if (data == NULL)
    {
        return;
    }
    if (ui_label_temp == NULL)
    {
        return; /* 页面尚未创建 */
    }

    //一律用十分位整数显示，避免依赖printf/%f（嵌入式系统浮点运算慢）
    temperature10 = (int)(data->temperature * 10.0f +  //温度转换为十分位（如23.5→235）
                          ((data->temperature >= 0.0f) ? 0.5f : -0.5f));  //四舍五入
    humidity10 = (int)(data->humidity * 10.0f + 0.5f);  //湿度转换为十分位并四舍五入
    light = (int)data->light;                           //光照直接取整
    //分离整数和小数部分
    t_int = temperature10 / 10;  //温度整数部分
    t_frac = temperature10 % 10; //温度小数部分（绝对值）
    h_int = humidity10 / 10;     //湿度整数部分
    h_frac = humidity10 % 10;    //湿度小数部分（绝对值）
    //确保小数部分为正数（避免负数取模结果为负）
    if (t_frac < 0) t_frac = -t_frac;
    if (h_frac < 0) h_frac = -h_frac;

    //更新温度显示（如T:23.5 C）
    lv_label_set_text_fmt(ui_label_temp, "T: %d.%d C", t_int, t_frac);
    //更新湿度显示（如H:45.2 %）
    lv_label_set_text_fmt(ui_label_humi, "H: %d.%d %%", h_int, h_frac);
    //更新光照显示（如L: 123）
    lv_label_set_text_fmt(ui_label_light, "L: %d", light);
}

//更新系统状态显示（改变状态文字和颜色）
void App_UI_UpdateState(SystemState_t state)
{
    const char *text;   //状态文本
    uint32_t color;     //状态颜色

    //检查UI标签是否已创建
    if (ui_label_state == NULL)
    {
        return;
    }

    //根据系统状态设置对应的文本和颜色
    switch (state)
    {
        case SYSTEM_STATE_ONLINE:
            text = "ONLINE";                   //在线
            color = 0x00E676U; /* 绿色 */    //绿色表示正常在线
            break;
        case SYSTEM_STATE_CACHING:
            text = "CACHING";                  //缓存中
            color = 0xFFEA00U; /* 黄色 */    //黄色表示正在保存数据
            break;
        case SYSTEM_STATE_ERROR:
            text = "ERROR";                    //错误
            color = 0xFF1744U; /* 红色 */    //红色表示出错
            break;
        default:
            text = "OFFLINE";                  //离线
            color = 0xFF8A50U; /* 橙色 */    //橙色表示离线状态
            break;
    }
    //更新状态文本颜色
    lv_obj_set_style_text_color(ui_label_state, lv_color_hex(color), 0U);
    //更新状态文本内容
    lv_label_set_text(ui_label_state, text);
}

//更新缓存计数显示
void App_UI_UpdateCacheCount(uint32_t count)
{
    //检查UI标签是否已创建
    if (ui_label_cache == NULL)
    {
        return;
    }
    //更新缓存计数显示（如CACHE:123）
    lv_label_set_text_fmt(ui_label_cache, "CACHE:%lu",
                          (unsigned long)count);
}

//更新 AI 判断结果显示（文字+颜色随状态变化）
void App_UI_UpdateAiState(uint8_t state, uint8_t valid)
{
    const char *text;   //状态文本
    uint32_t color;     //状态颜色

    //检查UI标签是否已创建
    if (ui_label_ai == NULL)
    {
        return;
    }

    if (valid == 0U)
    {
        text = "AI:--";                 //尚无有效推理结果
        color = 0x8A93A6U;              //灰色
    }
    else
    {
        //与 app_ai.h 的 AI_STATE_xxx 三类一一对应
        switch (state)
        {
            case AI_STATE_NORMAL:
                text = "AI:OK";         //正常
                color = 0x00E676U;      //绿色
                break;
            case AI_STATE_WARNING:
                text = "AI:WARN";       //预警（数值持续漂移）
                color = 0xFFEA00U;      //黄色
                break;
            case AI_STATE_FAULT:
                text = "AI:FAULT";      //故障（突跳/卡死）
                color = 0xFF1744U;      //红色
                break;
            default:
                text = "AI:--";         //未知类别，按未就绪显示
                color = 0x8A93A6U;
                break;
        }
    }
    //更新 AI 状态文字颜色
    lv_obj_set_style_text_color(ui_label_ai, lv_color_hex(color), 0U);
    //更新 AI 状态文字内容
    lv_label_set_text(ui_label_ai, text);
}
