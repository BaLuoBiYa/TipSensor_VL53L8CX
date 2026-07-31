#ifndef SHARED_BUF_H
#define SHARED_BUF_H

#include <stdint.h>
#include "drivers/parser.h"

/* ================================================================
 * 共享工作缓冲
 *   STM32G031 仅 8KB RAM，将互斥使用的大缓冲合并为一个 union：
 *   - pack 区    ：响应帧打包工作区（frame 构建区 + raw 输出区物理分离，
 *                  避免 memcpy 源/目的重叠）
 *   - spi_chunk  ：VL53L8CX SPI 传输工作区（chunk 1024 + 2B 地址头）
 *   主循环单线程顺序执行，各使用方互斥，无并发冲突。
 * ================================================================ */

#define SHARED_SPI_CHUNK_SIZE 1026 /* 1024 数据 + 2 地址头 */

typedef union {
    struct {
        uint8_t frame[sizeof(Frame_8x8_Full_t)]; /* 帧构建区（最大帧 710B） */
        uint8_t raw[sizeof(Frame_8x8_Full_t)];   /* 帧输出区               */
    } pack;
    uint8_t spi_chunk[SHARED_SPI_CHUNK_SIZE]; /* WrMulti/RdMulti 传输区  */
} Shared_Buffer_t;

extern Shared_Buffer_t g_shared;

#endif /* SHARED_BUF_H */
