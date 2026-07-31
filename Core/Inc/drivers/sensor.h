#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include "drivers/parser.h"
#include <vl53l8cx_api.h>

/* ================================================================
 * 返回帧模式选择（二选一）
 * ================================================================ */
// #define RESPONSE_MODE_FULL   /* 启用则返回完整帧，否则返回精简帧 */

/* ================================================================
 * 传感器数据缓冲（单缓冲，主循环阻塞操作）
 * ================================================================ */

typedef struct {
    VL53L8CX_ResultsData data; /* 传感器数据 */
    volatile bool data_ready;  /* ISR 置 true，主循环清 false */
    bool data_valid;           /* Collect 成功后置 true，PackResponse 后清 false */
    uint8_t resolution;        /* 当前分辨率 (16=4x4, 64=8x8) */
} Sensor_Buffer_t;

/**
 * @brief 初始化缓冲
 */
void SensorBuffer_Init(Sensor_Buffer_t *sb);

/**
 * @brief ISR 调用：仅置 data_ready 标志
 */
void SensorBuffer_NotifyISR(Sensor_Buffer_t *sb);

/**
 * @brief 主循环调用：阻塞读取传感器数据到缓冲
 * @param sb    Sensor_Buffer 实例
 * @param p_dev VL53L8CX 设备指针
 */
void SensorBuffer_Collect(Sensor_Buffer_t *sb, VL53L8CX_Configuration *p_dev);

/**
 * @brief 将缓冲中的传感器数据打包为响应帧
 * @param sb    Sensor_Buffer 实例
 * @param raw   输出缓冲区
 * @param id    传感器 ID
 * @return      打包后的帧总长；data_valid 为 false 时返回 0
 */
uint32_t SensorBuffer_PackResponse(Sensor_Buffer_t *sb, uint8_t *raw, uint8_t id);

#endif /* SENSOR_H */
