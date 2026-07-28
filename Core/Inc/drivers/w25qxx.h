#ifndef W25QXX_H
#define W25QXX_H

#include "main.h"
#include <stdbool.h>

typedef struct W25Qxx W25Qxx;

struct W25Qxx {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
};

void W25Qxx_Init(W25Qxx *flash, SPI_HandleTypeDef *hspi,
                 GPIO_TypeDef *cs_port, uint16_t cs_pin);

/* ── Basic read ──────────────────────────────────────────────────── */

uint8_t W25Qxx_Read(W25Qxx *flash, uint32_t addr, uint8_t *buf, uint32_t len);
uint8_t W25Qxx_Read_JEDEC_ID(W25Qxx *flash, uint8_t *id);

/* ── Write / Erase ────────────────────────────────────────────────── */

/* Wait until flash is no longer busy (poll Status Register BUSY bit) */
uint8_t W25Qxx_WaitBusy(W25Qxx *flash);

uint8_t W25Qxx_WriteEnable(W25Qxx *flash);
uint8_t W25Qxx_WriteDisable(W25Qxx *flash);

/* Page Program: up to 256 B, must not cross page boundary (addr & 0xFF + len ≤ 256) */
uint8_t W25Qxx_PageProgram(W25Qxx *flash, uint32_t addr,
                           const uint8_t *buf, uint32_t len);

/* Write arbitrary length (auto-splits across pages) */
uint8_t W25Qxx_Write(W25Qxx *flash, uint32_t addr,
                     const uint8_t *buf, uint32_t len);

/* Sector (4 KB), Block (64 KB), Chip Erase */
uint8_t W25Qxx_SectorErase(W25Qxx *flash, uint32_t addr);
uint8_t W25Qxx_BlockErase(W25Qxx *flash, uint32_t addr);
uint8_t W25Qxx_ChipErase(W25Qxx *flash);

#endif /* W25QXX_H */
