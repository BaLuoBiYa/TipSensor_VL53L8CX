#ifndef WS2812_H
#define WS2812_H

#include "main.h"
#include <stdint.h>

#define LED_NUM 1

/* ── PWM 编码常量 ──────────────────────────────────────────── */
/* TIM16_CLK=64MHz, Period=80 → PWM freq=800kHz, 1bit周期=1.25µs
 * WS2812B: T0H=200~500ns, T1H=550~850ns, Treset>50µs
 * BIT_0: CCR=26 → 高电平 26×(1/64M)=406ns, 低电平=844ns  */
#define WS2812_PWM_BIT_0 26
/* BIT_1: CCR=51 → 高电平 51×(1/64M)=797ns, 低电平=453ns  */
#define WS2812_PWM_BIT_1 51
/* 复位: 50 个零 CCR × 1.25µs = 62.5µs (>50µs)            */
#define WS2812_RESET_SLOTS 50

/* ── 结构体 ───────────────────────────────────────────────── */
typedef struct WS2812 WS2812;
typedef struct Pixel Pixel;

struct Pixel {
    uint16_t index;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct WS2812 {
    TIM_HandleTypeDef *htim;                            /* TIM 句柄     */
    uint16_t tx_buf[LED_NUM * 24 + WS2812_RESET_SLOTS]; /* CCR 缓冲区   */
};

/* ── 公共函数 ─────────────────────────────────────────────── */
void WS2812_Init(WS2812 *ws, TIM_HandleTypeDef *htim);
void WS2812_SetPixel(WS2812 *ws, Pixel *pixel);
void WS2812_Show(WS2812 *ws);
void WS2812_Ctrl(WS2812 *ws, uint8_t r, uint8_t g, uint8_t b);

#endif /* WS2812_H */
