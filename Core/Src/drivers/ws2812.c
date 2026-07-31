#include "drivers/ws2812.h"

/**
 * @brief 初始化 WS2812 实例
 * @param ws    WS2812 句柄
 * @param htim  TIM 句柄 (需配置为 800kHz PWM, DMA)
 */
void WS2812_Init(WS2812 *ws, TIM_HandleTypeDef *htim)
{
    ws->htim = htim;

    /* 填充复位脉冲 (CCR=0 → 输出持续低电平) */
    for (uint16_t i = 0; i < WS2812_RESET_SLOTS; i++) {
        ws->tx_buf[LED_NUM * 24 + i] = 0;
    }

    /* 默认全灭 */
    for (uint16_t i = 0; i < LED_NUM * 24; i++) {
        ws->tx_buf[i] = WS2812_PWM_BIT_0;
    }
}

/**
 * @brief 设置单个像素颜色 (GRB 顺序)
 * @param ws     WS2812 句柄
 * @param pixel  像素数据 (index, r, g, b)
 */
void WS2812_SetPixel(WS2812 *ws, Pixel *pixel)
{
    if (pixel->index >= LED_NUM) {
        return;
    }

    /* WS2812 数据顺序: G → R → B, MSB first */
    uint32_t grb = ((uint32_t)(pixel->g) << 16) | ((uint32_t)(pixel->r) << 8) | ((uint32_t)(pixel->b));

    uint16_t base = pixel->index * 24;
    for (uint8_t i = 0; i < 24; i++) {
        ws->tx_buf[base + i] = (grb & (1 << (23 - i)))
                                   ? WS2812_PWM_BIT_1
                                   : WS2812_PWM_BIT_0;
    }
}

/**
 * @brief 通过 DMA 发送缓冲区到 WS2812
 * @note  调用时机: 确保上一次 DMA 已完成再调用
 * @param ws  WS2812 句柄
 */
void WS2812_Show(WS2812 *ws)
{
    uint32_t len = LED_NUM * 24 + WS2812_RESET_SLOTS;
    HAL_TIM_PWM_Start_DMA(ws->htim, TIM_CHANNEL_1,
                          (uint32_t *)ws->tx_buf, len);
}

/**
 * @brief 快捷控制: 直接设置第 0 个 LED 颜色并发送
 * @param ws  WS2812 句柄
 * @param r   红色 (0-255)
 * @param g   绿色 (0-255)
 * @param b   蓝色 (0-255)
 */
void WS2812_Ctrl(WS2812 *ws, uint8_t r, uint8_t g, uint8_t b)
{
    Pixel px = {.index = 0, .r = r, .g = g, .b = b};
    WS2812_SetPixel(ws, &px);
    WS2812_Show(ws);
}
