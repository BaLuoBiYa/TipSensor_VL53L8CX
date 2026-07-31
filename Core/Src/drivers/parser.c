#include "drivers/parser.h"
#include "crc.h"
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
    return true;
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
 * 配置持久化（W25Q Flash，直接复用 Frame_Config_t）
 * ================================================================ */

bool Config_LoadFromFlash(Frame_Config_t *cfg, struct W25Qxx *flash)
{
    if (W25Qxx_Read(flash, CONFIG_FLASH_ADDR, (uint8_t *)cfg, sizeof(Frame_Config_t)) != 0) {
        return false;
    }
    /* 以 header.magic 作为有效性标记 */
    return cfg->header.magic[0] == FRAME_MAGIC_0 && cfg->header.magic[1] == FRAME_MAGIC_1;
}

bool Config_SaveToFlash(const Frame_Config_t *cfg, struct W25Qxx *flash)
{
    uint8_t sector[4096];
    uint32_t sector_addr = CONFIG_FLASH_ADDR & ~0xFFF;
    uint32_t offset      = CONFIG_FLASH_ADDR - sector_addr;

    if (W25Qxx_Read(flash, sector_addr, sector, sizeof(sector)) != 0) {
        return false;
    }

    memcpy(sector + offset, cfg, sizeof(Frame_Config_t));

    if (W25Qxx_SectorErase(flash, sector_addr) != 0) {
        return false;
    }
    return W25Qxx_Write(flash, sector_addr, sector, sizeof(sector)) == 0;
}
