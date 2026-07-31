#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>
#include <stdbool.h>

#include "drivers/w25qxx.h"

/* ================================================================
 * 帧模式枚举
 * ================================================================ */
typedef enum {
    FRAME_MODE_CONFIG   = 0, /* 主机→传感器：传感器配置帧 */
    FRAME_MODE_REQUEST  = 1, /* 主机→传感器：传感器请求帧 */
    FRAME_MODE_4X4_SLIM = 2, /* 传感器→主机：4x4精简返回帧 */
    FRAME_MODE_4X4_FULL = 3, /* 传感器→主机：4x4完整返回帧 */
    FRAME_MODE_8X8_SLIM = 4, /* 传感器→主机：8x8精简返回帧 */
    FRAME_MODE_8X8_FULL = 5, /* 传感器→主机：8x8完整返回帧 */
} Frame_Mode_t;

/* ================================================================
 * 帧头 magic 常量
 * ================================================================ */
#define FRAME_MAGIC_0 0xFE
#define FRAME_MAGIC_1 0xEE

/* 广播 ID */
#define FRAME_ID_BROADCAST 15

#pragma pack(push, 1)

/* ================================================================
 * 公共帧头 (3 Byte)
 *   byte[0-1]: magic
 *   byte[2]:   bit[0-3] = id, bit[4-7] = mode
 * ================================================================ */
typedef struct {
    uint8_t magic[2];
    uint8_t id_mode; /* bit0-3: id, bit4-7: mode */
} Frame_Header_t;

/* ================================================================
 * 单 zone 完整数据 (11 Byte)
 * ================================================================ */
typedef struct {
    uint8_t nb_target_detected; /* 该 zone 检测到的有效目标数 */
    uint32_t signal_per_spad;   /* 回波信号强度，单位 kcps/spad */
    uint16_t range_sigma_mm;    /* 测距标准差，单位 mm */
    int16_t distance_mm;        /* 测量距离，单位 mm */
    uint8_t reflectance;        /* 估计反射率，单位 % */
    uint8_t target_status;      /* 测量有效性状态 (5 或 9 表示有效) */
} Zone_Data_t;

/* ================================================================
 * 主机→传感器：传感器配置帧 (mode=0, 8 Byte)
 * ================================================================ */
typedef struct {
    Frame_Header_t header;
    union {
        uint8_t raw;
        struct {
            uint8_t resolution : 1;   /* 0=4x4, 1=8x8 */
            uint8_t target_order : 1; /* 0=最强回波, 1=最近回波 */
            uint8_t framerate : 6;    /* 帧率 */
        } bits;
    } payload;
    uint16_t sharpener; /* 锐化器系数 0~99 */
    uint16_t crc16;
} Frame_Config_t;

/* ================================================================
 * 主机→传感器：传感器请求帧 (mode=1, 5 Byte)
 * ================================================================ */
typedef struct {
    Frame_Header_t header;
    uint16_t crc16;
} Frame_Request_t;

/* ================================================================
 * 传感器→主机：4x4精简返回帧 (mode=2, 37 Byte)
 * ================================================================ */
typedef struct {
    Frame_Header_t header;
    uint16_t distance[16]; /* 深度图，单位 mm */
    uint16_t crc16;
} Frame_4x4_Slim_t;

/* ================================================================
 * 传感器→主机：4x4完整返回帧 (mode=3, 182 Byte)
 * ================================================================ */
typedef struct {
    Frame_Header_t header;
    int8_t silicon_temp_degc; /* 芯片内部温度，单位 ℃ */
    Zone_Data_t zones[16];    /* 16 个 zone 的完整数据 */
    uint16_t crc16;
} Frame_4x4_Full_t;

/* ================================================================
 * 传感器→主机：8x8精简返回帧 (mode=4, 133 Byte)
 * ================================================================ */
typedef struct {
    Frame_Header_t header;
    uint16_t distance[64]; /* 深度图，单位 mm */
    uint16_t crc16;
} Frame_8x8_Slim_t;

/* ================================================================
 * 传感器→主机：8x8完整返回帧 (mode=5, 710 Byte)
 * ================================================================ */
typedef struct {
    Frame_Header_t header;
    int8_t silicon_temp_degc; /* 芯片内部温度，单位 ℃ */
    Zone_Data_t zones[64];    /* 64 个 zone 的完整数据 */
    uint16_t crc16;
} Frame_8x8_Full_t;

#pragma pack(pop)

/* ================================================================
 * 编译期大小校验
 * ================================================================ */
_Static_assert(sizeof(Frame_Header_t) == 3, "Frame_Header_t size mismatch");
_Static_assert(sizeof(Zone_Data_t) == 11, "Zone_Data_t size mismatch");
_Static_assert(sizeof(Frame_Config_t) == 8, "Frame_Config_t size mismatch");
_Static_assert(sizeof(Frame_Request_t) == 5, "Frame_Request_t size mismatch");
_Static_assert(sizeof(Frame_4x4_Slim_t) == 37, "Frame_4x4_Slim_t size mismatch");
_Static_assert(sizeof(Frame_4x4_Full_t) == 182, "Frame_4x4_Full_t size mismatch");
_Static_assert(sizeof(Frame_8x8_Slim_t) == 133, "Frame_8x8_Slim_t size mismatch");
_Static_assert(sizeof(Frame_8x8_Full_t) == 710, "Frame_8x8_Full_t size mismatch");

/* ================================================================
 * Header 分流
 * ================================================================ */

/**
 * @brief 解析 3 Byte 帧头，校验 magic 并提取 ID 和 mode
 * @param raw  输入字节流（至少 3 Byte）
 * @param id   输出传感器 ID (0~14 / 15=广播)
 * @param mode 输出帧模式
 * @return     true=magic 匹配，header 有效
 */
bool Frame_Parse_Header(const uint8_t *raw, uint8_t *id, Frame_Mode_t *mode);

/**
 * @brief 根据 mode 返回该帧期望的总字节数（含 CRC）
 * @param mode  帧模式
 * @return      期望的总帧长；未知 mode 返回 0
 */
uint32_t Frame_ExpectedSize(Frame_Mode_t mode);

/**
 * @brief 校验帧总长是否匹配其 mode 的期望长度
 * @param raw_len  接收缓冲中的有效字节数
 * @param mode     帧模式
 * @return         true=长度匹配
 */
bool Frame_ValidateSize(uint32_t raw_len, Frame_Mode_t mode);

/* ================================================================
 * 打包/解析函数
 *   所有 Pack 函数自动填充 header.magic + 计算 CRC，返回帧总长
 *   所有 Parse 函数自动校验 magic + CRC，返回 true/false
 * ================================================================ */

uint32_t Frame_Pack_Config(const Frame_Config_t *frame, uint8_t *raw, uint8_t id);
bool Frame_Parse_Config(const uint8_t *raw, Frame_Config_t *frame);

/**
 * @brief 校验配置参数合法性（与协议文档一致）
 *        sharpener: 0~99；framerate: 4x4 ≤ 60Hz，8x8 ≤ 15Hz
 * @return true=参数合法
 */
bool Config_Validate(const Frame_Config_t *frame);

uint32_t Frame_Pack_Request(const Frame_Request_t *frame, uint8_t *raw, uint8_t id);
bool Frame_Parse_Request(const uint8_t *raw, Frame_Request_t *frame);

uint32_t Frame_Pack_4x4_Slim(const Frame_4x4_Slim_t *frame, uint8_t *raw, uint8_t id);
bool Frame_Parse_4x4_Slim(const uint8_t *raw, Frame_4x4_Slim_t *frame);

uint32_t Frame_Pack_4x4_Full(const Frame_4x4_Full_t *frame, uint8_t *raw, uint8_t id);
bool Frame_Parse_4x4_Full(const uint8_t *raw, Frame_4x4_Full_t *frame);

uint32_t Frame_Pack_8x8_Slim(const Frame_8x8_Slim_t *frame, uint8_t *raw, uint8_t id);
bool Frame_Parse_8x8_Slim(const uint8_t *raw, Frame_8x8_Slim_t *frame);

uint32_t Frame_Pack_8x8_Full(const Frame_8x8_Full_t *frame, uint8_t *raw, uint8_t id);
bool Frame_Parse_8x8_Full(const uint8_t *raw, Frame_8x8_Full_t *frame);

/* ================================================================
 * 配置持久化（W25Q Flash）
 *   直接复用 Frame_Config_t 结构体存取，以 header.magic 作有效性标记
 * ================================================================ */

#define CONFIG_FLASH_ADDR 0x020000 /* Flash 存储地址（64 KB 边界，VL53L8CX FW 之后） */

/**
 * @brief 从 Flash 加载 Frame_Config_t 配置
 * @param cfg   输出配置帧（仅 payload + sharpener 有效，header/crc 用于校验）
 * @param flash W25Q 实例指针
 * @return      true=读取成功且 header.magic 匹配
 */
bool Config_LoadFromFlash(Frame_Config_t *cfg, struct W25Qxx *flash);

/**
 * @brief 将 Frame_Config_t 写入 Flash（自动擦除 sector 后写入）
 * @param cfg   待写入配置帧
 * @param flash W25Q 实例指针
 * @return      true=写入成功
 */
bool Config_SaveToFlash(const Frame_Config_t *cfg, struct W25Qxx *flash);

#endif /* PARSER_H */
