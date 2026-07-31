#include "drivers/parser.h"
#include "crc.h"
#include <stdbool.h>
#include <string.h>

/* ================================================================
 * 内部辅助（static，不对外暴露）
 * ================================================================ */

static uint16_t calc_crc16(const uint8_t *data, uint32_t len)
{
    __HAL_CRC_DR_RESET(&hcrc);
    return (uint16_t)HAL_CRC_Accumulate(&hcrc, (uint32_t *)data, len);
}

static uint16_t write_crc16(uint8_t *raw, uint32_t len)
{
    uint16_t crc = calc_crc16(raw, len);
    raw[len]     = (uint8_t)(crc & 0xFF);
    raw[len + 1] = (uint8_t)(crc >> 8);
    return crc;
}

static bool check_crc16(const uint8_t *raw, uint32_t total_len)
{
    if (total_len < 2) {
        return false;
    }
    uint32_t len      = total_len - 2;
    uint16_t expected = calc_crc16(raw, len);
    uint16_t actual   = (uint16_t)raw[len] | ((uint16_t)raw[len + 1] << 8);
    return expected == actual;
}

static void pack_zone(const Zone_Data_t *zone, uint8_t *raw)
{
    memcpy(raw, zone, sizeof(Zone_Data_t));
}

static void parse_zone(const uint8_t *raw, Zone_Data_t *zone)
{
    memcpy(zone, raw, sizeof(Zone_Data_t));
}

static void write_header(uint8_t *raw, uint8_t id, uint8_t mode)
{
    raw[0] = FRAME_MAGIC_0;
    raw[1] = FRAME_MAGIC_1;
    raw[2] = (uint8_t)((mode << 4) | (id & 0x0F));
}

static bool check_magic(const uint8_t *raw)
{
    return raw[0] == FRAME_MAGIC_0 && raw[1] == FRAME_MAGIC_1;
}

/* ================================================================
 * Header 解析
 * ================================================================ */

bool Frame_Parse_Header(const uint8_t *raw, uint8_t *id, Frame_Mode_t *mode)
{
    if (!check_magic(raw)) {
        return false;
    }
    *id   = raw[2] & 0x0F;
    *mode = (Frame_Mode_t)(raw[2] >> 4);
    return true;
}

uint32_t Frame_ExpectedSize(Frame_Mode_t mode)
{
    switch (mode) {
        case FRAME_MODE_CONFIG:
            return sizeof(Frame_Config_t);
        case FRAME_MODE_REQUEST:
            return sizeof(Frame_Request_t);
        case FRAME_MODE_4X4_SLIM:
            return sizeof(Frame_4x4_Slim_t);
        case FRAME_MODE_4X4_FULL:
            return sizeof(Frame_4x4_Full_t);
        case FRAME_MODE_8X8_SLIM:
            return sizeof(Frame_8x8_Slim_t);
        case FRAME_MODE_8X8_FULL:
            return sizeof(Frame_8x8_Full_t);
        default:
            return 0;
    }
}

bool Frame_ValidateSize(uint32_t raw_len, Frame_Mode_t mode)
{
    return raw_len == Frame_ExpectedSize(mode);
}

/* ================================================================
 * Frame_Pack_Config / Frame_Parse_Config (mode=0, 8 Byte)
 * ================================================================ */

/**
 * @brief 校验配置参数合法性（与协议文档一致）
 *        sharpener: 0~99；framerate: 4x4 ≤ 60Hz，8x8 ≤ 15Hz
 * @return true=参数合法
 */
bool Config_Validate(const Frame_Config_t *frame)
{
    if (frame->sharpener > 99) {
        return false;
    }
    if (frame->payload.bits.framerate < 1) {
        return false;
    }
    if (frame->payload.bits.resolution == 0) {
        if (frame->payload.bits.framerate > 60) {
            return false;
        }
    } else {
        if (frame->payload.bits.framerate > 15) {
            return false;
        }
    }
    return true;
}

uint32_t Frame_Pack_Config(const Frame_Config_t *frame, uint8_t *raw, uint8_t id)
{
    write_header(raw, id, FRAME_MODE_CONFIG);
    raw[3] = frame->payload.raw;
    memcpy(raw + 4, &frame->sharpener, sizeof(uint16_t));
    write_crc16(raw, 6);
    return sizeof(Frame_Config_t);
}

bool Frame_Parse_Config(const uint8_t *raw, Frame_Config_t *frame)
{
    if (!check_magic(raw)) {
        return false;
    }
    if (!check_crc16(raw, sizeof(Frame_Config_t))) {
        return false;
    }

    frame->header.magic[0] = raw[0];
    frame->header.magic[1] = raw[1];
    frame->header.id_mode  = raw[2];
    frame->payload.raw     = raw[3];
    memcpy(&frame->sharpener, raw + 4, sizeof(uint16_t));
    memcpy(&frame->crc16, raw + 6, sizeof(uint16_t));

    /* 参数合法性检查 */
    return Config_Validate(frame);
}

/* ================================================================
 * Frame_Pack_Request / Frame_Parse_Request (mode=1, 5 Byte)
 * ================================================================ */

uint32_t Frame_Pack_Request(const Frame_Request_t *frame, uint8_t *raw, uint8_t id)
{
    (void)frame;
    write_header(raw, id, FRAME_MODE_REQUEST);
    write_crc16(raw, 3);
    return sizeof(Frame_Request_t);
}

bool Frame_Parse_Request(const uint8_t *raw, Frame_Request_t *frame)
{
    if (!check_magic(raw)) {
        return false;
    }
    if (!check_crc16(raw, sizeof(Frame_Request_t))) {
        return false;
    }

    frame->header.magic[0] = raw[0];
    frame->header.magic[1] = raw[1];
    frame->header.id_mode  = raw[2];
    memcpy(&frame->crc16, raw + 3, sizeof(uint16_t));
    return true;
}

/* ================================================================
 * Frame_Pack_4x4_Slim / Frame_Parse_4x4_Slim (mode=2, 37 Byte)
 * ================================================================ */

uint32_t Frame_Pack_4x4_Slim(const Frame_4x4_Slim_t *frame, uint8_t *raw, uint8_t id)
{
    write_header(raw, id, FRAME_MODE_4X4_SLIM);
    memcpy(raw + 3, frame->distance, sizeof(frame->distance));
    write_crc16(raw, 35);
    return sizeof(Frame_4x4_Slim_t);
}

bool Frame_Parse_4x4_Slim(const uint8_t *raw, Frame_4x4_Slim_t *frame)
{
    if (!check_magic(raw)) {
        return false;
    }
    if (!check_crc16(raw, sizeof(Frame_4x4_Slim_t))) {
        return false;
    }

    frame->header.magic[0] = raw[0];
    frame->header.magic[1] = raw[1];
    frame->header.id_mode  = raw[2];
    memcpy(frame->distance, raw + 3, sizeof(frame->distance));
    memcpy(&frame->crc16, raw + 35, sizeof(uint16_t));
    return true;
}

/* ================================================================
 * Frame_Pack_4x4_Full / Frame_Parse_4x4_Full (mode=3, 182 Byte)
 * ================================================================ */

uint32_t Frame_Pack_4x4_Full(const Frame_4x4_Full_t *frame, uint8_t *raw, uint8_t id)
{
    write_header(raw, id, FRAME_MODE_4X4_FULL);
    raw[3] = (uint8_t)frame->silicon_temp_degc;
    for (int i = 0; i < 16; i++) {
        pack_zone(&frame->zones[i], raw + 4 + i * sizeof(Zone_Data_t));
    }
    write_crc16(raw, 180);
    return sizeof(Frame_4x4_Full_t);
}

bool Frame_Parse_4x4_Full(const uint8_t *raw, Frame_4x4_Full_t *frame)
{
    if (!check_magic(raw)) {
        return false;
    };
    if (!check_crc16(raw, sizeof(Frame_4x4_Full_t))) {
        return false;
    };

    frame->header.magic[0]   = raw[0];
    frame->header.magic[1]   = raw[1];
    frame->header.id_mode    = raw[2];
    frame->silicon_temp_degc = (int8_t)raw[3];
    for (int i = 0; i < 16; i++) {
        parse_zone(raw + 4 + i * sizeof(Zone_Data_t), &frame->zones[i]);
    }
    memcpy(&frame->crc16, raw + 180, sizeof(uint16_t));
    return true;
}

/* ================================================================
 * Frame_Pack_8x8_Slim / Frame_Parse_8x8_Slim (mode=4, 133 Byte)
 * ================================================================ */

uint32_t Frame_Pack_8x8_Slim(const Frame_8x8_Slim_t *frame, uint8_t *raw, uint8_t id)
{
    write_header(raw, id, FRAME_MODE_8X8_SLIM);
    memcpy(raw + 3, frame->distance, sizeof(frame->distance));
    write_crc16(raw, 131);
    return sizeof(Frame_8x8_Slim_t);
}

bool Frame_Parse_8x8_Slim(const uint8_t *raw, Frame_8x8_Slim_t *frame)
{
    if (!check_magic(raw)) {
        return false;
    };
    if (!check_crc16(raw, sizeof(Frame_8x8_Slim_t))) {
        return false;
    };

    frame->header.magic[0] = raw[0];
    frame->header.magic[1] = raw[1];
    frame->header.id_mode  = raw[2];
    memcpy(frame->distance, raw + 3, sizeof(frame->distance));
    memcpy(&frame->crc16, raw + 131, sizeof(uint16_t));
    return true;
}

/* ================================================================
 * Frame_Pack_8x8_Full / Frame_Parse_8x8_Full (mode=5, 710 Byte)
 * ================================================================ */

uint32_t Frame_Pack_8x8_Full(const Frame_8x8_Full_t *frame, uint8_t *raw, uint8_t id)
{
    write_header(raw, id, FRAME_MODE_8X8_FULL);
    raw[3] = (uint8_t)frame->silicon_temp_degc;
    for (int i = 0; i < 64; i++) {
        pack_zone(&frame->zones[i], raw + 4 + i * sizeof(Zone_Data_t));
    }
    write_crc16(raw, 708);
    return sizeof(Frame_8x8_Full_t);
}

bool Frame_Parse_8x8_Full(const uint8_t *raw, Frame_8x8_Full_t *frame)
{
    if (!check_magic(raw)) {
        return false;
    };
    if (!check_crc16(raw, sizeof(Frame_8x8_Full_t))) {
        return false;
    };

    frame->header.magic[0]   = raw[0];
    frame->header.magic[1]   = raw[1];
    frame->header.id_mode    = raw[2];
    frame->silicon_temp_degc = (int8_t)raw[3];
    for (int i = 0; i < 64; i++) {
        parse_zone(raw + 4 + i * sizeof(Zone_Data_t), &frame->zones[i]);
    }
    memcpy(&frame->crc16, raw + 708, sizeof(uint16_t));
    return true;
}

/* ================================================================
 * 配置持久化（W25Q Flash）
 *   页槽位方案：每槽 8 Byte，顺序写入扇区，写满才擦除。
 *   - 无 4KB 扇区 RAM 缓冲（栈/静态零大数组）
 *   - Flash 磨损降 16 倍（512 槽写满才擦一次）
 *   - Load 时校验 CRC16（旧实现只验 magic）
 * ================================================================ */

#define CONFIG_SLOT_SIZE  (sizeof(Frame_Config_t))  /* 8 Byte */
#define CONFIG_SLOT_COUNT (4096 / CONFIG_SLOT_SIZE) /* 512 槽 */

bool Config_LoadFromFlash(Frame_Config_t *cfg, struct W25Qxx *flash)
{
    bool found = false;

    /* 从槽 0 向后扫描：遇到空闲槽(首字节非 magic)即停止 */
    for (uint32_t i = 0; i < CONFIG_SLOT_COUNT; i++) {
        uint8_t slot[CONFIG_SLOT_SIZE];
        uint32_t addr = CONFIG_FLASH_ADDR + i * CONFIG_SLOT_SIZE;

        if (W25Qxx_Read(flash, addr, slot, sizeof(slot)) != 0) {
            return false;
        }
        /* 空闲槽标记（扇区擦除后为 0xFF） */
        if (slot[0] != FRAME_MAGIC_0 || slot[1] != FRAME_MAGIC_1) {
            break;
        }
        /* magic 匹配：CRC 通过才视为有效配置（取最新） */
        if (check_crc16(slot, CONFIG_SLOT_SIZE)) {
            memcpy(cfg, slot, sizeof(Frame_Config_t));
            found = true;
        }
    }

    return found;
}

bool Config_SaveToFlash(const Frame_Config_t *cfg, struct W25Qxx *flash)
{
    /* 找第一个空闲槽 */
    for (uint32_t i = 0; i < CONFIG_SLOT_COUNT; i++) {
        uint8_t head;
        uint32_t addr = CONFIG_FLASH_ADDR + i * CONFIG_SLOT_SIZE;

        if (W25Qxx_Read(flash, addr, &head, 1) != 0) {
            return false;
        }
        if (head == 0xFF) {
            /* 空闲槽：仅写 8 Byte（单页编程） */
            return W25Qxx_Write(flash, addr, (const uint8_t *)cfg,
                                CONFIG_SLOT_SIZE) == 0;
        }
    }

    /* 512 槽全部写满：擦除整扇区后写槽 0 */
    if (W25Qxx_SectorErase(flash, CONFIG_FLASH_ADDR) != 0) {
        return false;
    }
    return W25Qxx_Write(flash, CONFIG_FLASH_ADDR, (const uint8_t *)cfg,
                        CONFIG_SLOT_SIZE) == 0;
}
