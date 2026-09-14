#include "app_ai.h"
#include "network.h"
#include "network_data.h"
#include "network_data_params.h"
#include <string.h>

/* =====================================================================
 * 边缘 AI 传感器异常检测应用层
 *
 * 数据流：
 *   UITask 收到新传感器数据 -> App_AI_Feed()   更新 8 点滑动窗口
 *   UITask 循环调用 App_AI_Process()           每 2 秒触发一次推理
 *   推理流程：提取8维特征 -> 归一化 -> 转int8 -> ai_network_run -> 取argmax
 *   结果存入 ai_result，由 App_UI_UpdateAiState() 显示到 LVGL
 *
 * 特征定义（必须与训练脚本 train_sensor_anomaly.py 完全一致，顺序不可调换）：
 *   [0] 当前温度      [1] 当前湿度      [2] 当前光照
 *   [3] 温度变化量    [4] 湿度变化量    [5] 光照变化量
 *   [6] 温度8点均值   [7] 湿度8点均值
 * ===================================================================== */

/* ---------------- 滑动窗口（8 个采样点） ---------------- */
#define AI_WINDOW_SIZE      8U

static float ai_temp_buf[AI_WINDOW_SIZE];   /* 温度历史窗口 */
static float ai_humi_buf[AI_WINDOW_SIZE];   /* 湿度历史窗口 */
static float ai_light_buf[AI_WINDOW_SIZE];  /* 光照历史窗口 */
static uint8_t ai_window_count;             /* 已填入的采样数（0~8） */

/* ---------------- 归一化参数（与 ai_norm_params.h / 训练脚本一致） ---------------- */
#define AI_NORM_TEMP_OFFSET      (-40.0f)
#define AI_NORM_TEMP_SCALE       (165.0f)
#define AI_NORM_HUMI_OFFSET      (0.0f)
#define AI_NORM_HUMI_SCALE       (100.0f)
#define AI_NORM_LIGHT_OFFSET     (0.0f)
#define AI_NORM_LIGHT_SCALE      (2000.0f)
#define AI_NORM_DELTA_OFFSET     (-10.0f)
#define AI_NORM_DELTA_SCALE      (20.0f)
#define AI_NORM_LDELTA_OFFSET    (-500.0f)
#define AI_NORM_LDELTA_SCALE     (1000.0f)

/* ---------------- Cube.AI 句柄与缓冲区 ---------------- */
/* 激活内存约 1KB，由 AI_NETWORK_DATA_ACTIVATIONS_SIZE 指定，对齐到 4 字节 */
static AI_ALIGNED(4)
uint8_t ai_activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];

static ai_handle ai_network = AI_HANDLE_NULL;   /* 网络句柄 */
static uint8_t ai_initialized;                  /* 网络初始化完成标志 */

/* 输入量化参数（来自 network.c 中输入 tensor 的 intq 元数据）
 * int8 实际值 = (quant_value - zero_point) * scale */
#define AI_IN_SCALE       0.002502752f
#define AI_IN_ZEROPOINT   (-128)
/* 输出量化参数（nl_3_output_array_intq：scale=0.00390625, zp=-128）
 * softmax 输出恒 >= 0，因此实际概率 = (q + 128) * 0.00390625 */
#define AI_OUT_SCALE      0.00390625f
#define AI_OUT_ZEROPOINT  (-128)

/* ---------------- 推理节流 ---------------- */
#define AI_PERIOD_MS      2000U   /* 推理周期：2 秒一次 */
static uint32_t ai_last_run_tick;

/* ---------------- 最近一次推理结果 ---------------- */
static AiResult_t ai_result;

/* ---------------------------------------------------------------------
 * 网络初始化：只执行一次，失败则置 error 并不再重试（UI 显示保持默认）
 * ------------------------------------------------------------------- */
static uint8_t App_AI_InitNetwork(void)
{
    ai_error err;
    ai_handle act[1];   /* 激活缓冲区句柄数组：create_and_init 按下标取地址 */

    /* 激活内存必须 4 字节对齐，数组元素才是真正的缓冲区地址 */
    act[0] = AI_HANDLE_PTR(ai_activations);

    /* create_and_init：分配网络上下文并绑定权重（Flash）与激活（RAM）。
     * 权重传 NULL：codegen 的 map 已静态绑定 s_network_weights_array_u64 */
    err = ai_network_create_and_init(&ai_network, act, NULL);
    if (err.type != AI_ERROR_NONE)
    {
        ai_result.error_count++;
        return 0U;
    }
    ai_initialized = 1U;
    return 1U;
}

/* ---------------------------------------------------------------------
 * 喂入一帧新的传感器数据，维护 8 点滑动窗口
 * 在 UITask 收到 lvglDataQueue 数据后调用（同一个任务上下文，无需加锁）
 * ------------------------------------------------------------------- */
void App_AI_Feed(const GatewayData_t *data)
{
    uint8_t i;

    if (data == NULL)
    {
        return;
    }

    /* 窗口未满前整体后移一位；满了之后丢最旧、追加最新（简单滑动窗口） */
    if (ai_window_count < AI_WINDOW_SIZE)
    {
        i = ai_window_count;
        ai_temp_buf[i] = data->temperature;
        ai_humi_buf[i] = data->humidity;
        ai_light_buf[i] = data->light;
        ai_window_count++;
    }
    else
    {
        for (i = 0U; i < AI_WINDOW_SIZE - 1U; i++)
        {
            ai_temp_buf[i] = ai_temp_buf[i + 1U];
            ai_humi_buf[i] = ai_humi_buf[i + 1U];
            ai_light_buf[i] = ai_light_buf[i + 1U];
        }
        ai_temp_buf[AI_WINDOW_SIZE - 1U] = data->temperature;
        ai_humi_buf[AI_WINDOW_SIZE - 1U] = data->humidity;
        ai_light_buf[AI_WINDOW_SIZE - 1U] = data->light;
    }
}

/* ---------------------------------------------------------------------
 * 从滑动窗口提取 8 维特征（与训练脚本 extract_features() 逐行对应）
 * ------------------------------------------------------------------- */
static void App_AI_ExtractFeatures(float *feat)
{
    float temp_mean = 0.0f;
    float humi_mean = 0.0f;
    uint8_t i;

    /* 均值：对窗口内全部已填入的样本求平均 */
    for (i = 0U; i < AI_WINDOW_SIZE; i++)
    {
        temp_mean += ai_temp_buf[i];
        humi_mean += ai_humi_buf[i];
    }
    temp_mean /= (float)AI_WINDOW_SIZE;
    humi_mean /= (float)AI_WINDOW_SIZE;

    feat[0] = ai_temp_buf[AI_WINDOW_SIZE - 1U];   /* 当前温度 */
    feat[1] = ai_humi_buf[AI_WINDOW_SIZE - 1U];   /* 当前湿度 */
    feat[2] = ai_light_buf[AI_WINDOW_SIZE - 1U];  /* 当前光照 */
    feat[3] = ai_temp_buf[AI_WINDOW_SIZE - 1U] - ai_temp_buf[AI_WINDOW_SIZE - 2U];  /* 温度变化量 */
    feat[4] = ai_humi_buf[AI_WINDOW_SIZE - 1U] - ai_humi_buf[AI_WINDOW_SIZE - 2U];  /* 湿度变化量 */
    feat[5] = ai_light_buf[AI_WINDOW_SIZE - 1U] - ai_light_buf[AI_WINDOW_SIZE - 2U]; /* 光照变化量 */
    feat[6] = temp_mean;                          /* 温度均值 */
    feat[7] = humi_mean;                          /* 湿度均值 */
}

/* ---------------------------------------------------------------------
 * 归一化到 0~1 并线性量化为 int8（与训练端 INT8 量化对齐）
 * quant = round(x / in_scale) + in_zero_point
 * ------------------------------------------------------------------- */
static void App_AI_QuantizeInput(const float *feat, int8_t *input)
{
    static const float offset[8] = {
        AI_NORM_TEMP_OFFSET, AI_NORM_HUMI_OFFSET, AI_NORM_LIGHT_OFFSET,
        AI_NORM_DELTA_OFFSET, AI_NORM_DELTA_OFFSET, AI_NORM_LDELTA_OFFSET,
        AI_NORM_TEMP_OFFSET, AI_NORM_HUMI_OFFSET
    };
    static const float scale[8] = {
        AI_NORM_TEMP_SCALE, AI_NORM_HUMI_SCALE, AI_NORM_LIGHT_SCALE,
        AI_NORM_DELTA_SCALE, AI_NORM_DELTA_SCALE, AI_NORM_LDELTA_SCALE,
        AI_NORM_TEMP_SCALE, AI_NORM_HUMI_SCALE
    };
    uint8_t i;
    float x;
    int q;

    for (i = 0U; i < 8U; i++)
    {
        /* 归一化：(特征 - offset) / scale，并截断到 0~1（与 Python 端 clip 一致） */
        x = (feat[i] - offset[i]) / scale[i];
        if (x < 0.0f) { x = 0.0f; }
        if (x > 1.0f) { x = 1.0f; }

        /* float -> int8 量化：q = round(x/scale) + zero_point，最后夹紧 */
        q = (int)(x / AI_IN_SCALE + (x >= 0.0f ? 0.5f : -0.5f)) + AI_IN_ZEROPOINT;
        if (q < -128) { q = -128; }
        if (q > 127)  { q = 127; }
        input[i] = (int8_t)q;
    }
}

/* ---------------------------------------------------------------------
 * 执行一次完整推理：特征 -> 量化 -> run -> 反量化 -> argmax
 * 返回 1 = 成功得到结果；0 = 失败（网络未初始化或 run 报错）
 * ------------------------------------------------------------------- */
static uint8_t App_AI_Infer(void)
{
    float feat[8];
    int8_t input_data[8];
    ai_buffer *in_buf;
    ai_buffer *out_buf;
    ai_i32 run_ret;
    const int8_t *out_data;
    float prob[3];
    uint8_t best;
    uint8_t i;

    /* 窗口未填满前不推理，避免用残缺数据得出错误结论 */
    if (ai_window_count < AI_WINDOW_SIZE)
    {
        return 0U;
    }

    /* 首次调用时初始化网络（放这里是因为 UITask 首帧数据到达时外设已就绪） */
    if (ai_initialized == 0U)
    {
        if (App_AI_InitNetwork() == 0U)
        {
            return 0U;
        }
    }

    App_AI_ExtractFeatures(feat);
    App_AI_QuantizeInput(feat, input_data);

    /* allocate-inputs/outputs 模式：输入输出都位于网络激活区内。
     * 通过 ai_network_inputs_get/outputs_get 拿到网络自己的 io buffer，
     * 把量化后的数据拷进输入 buffer 指向的地址，run 后从输出 buffer 读结果。 */
    in_buf = ai_network_inputs_get(ai_network, NULL);
    out_buf = ai_network_outputs_get(ai_network, NULL);
    if ((in_buf == NULL) || (out_buf == NULL))
    {
        ai_result.error_count++;
        return 0U;
    }

    /* 输入 8 个 int8，size 以元素个数计；把数据写入网络输入缓冲指向的内存 */
    memcpy((void *)in_buf->data, input_data, 8U);

    run_ret = ai_network_run(ai_network, in_buf, out_buf);
    if (run_ret != 1)
    {
        ai_result.error_count++;
        return 0U;
    }

    /* 从输出缓冲取 int8 结果并反量化为概率 */
    out_data = (const int8_t *)out_buf->data;
    best = 0U;
    for (i = 0U; i < 3U; i++)
    {
        prob[i] = ((float)out_data[i] - (float)AI_OUT_ZEROPOINT) * AI_OUT_SCALE;
        if (prob[i] > prob[best])
        {
            best = i;   /* argmax：取概率最大的类别 */
        }
    }

    ai_result.state = best;
    ai_result.score = prob[best];
    ai_result.run_count++;
    return 1U;
}

/* ---------------------------------------------------------------------
 * 周期推理入口：由 UITask 循环调用，内部按 AI_PERIOD_MS 节流
 * 返回 1 = 本次产生了新结果
 * ------------------------------------------------------------------- */
uint8_t App_AI_Process(AiResult_t *result)
{
    uint32_t now = HAL_GetTick();

    if ((now - ai_last_run_tick) < AI_PERIOD_MS)
    {
        return 0U;   /* 未到推理周期 */
    }
    ai_last_run_tick = now;

    if (App_AI_Infer() == 0U)
    {
        return 0U;
    }
    if (result != NULL)
    {
        *result = ai_result;
    }
    return 1U;
}

/* 读取最近一次推理结果（跨任务读取 float，量级小，不做加锁） */
AiResult_t App_AI_GetResult(void)
{
    return ai_result;
}
