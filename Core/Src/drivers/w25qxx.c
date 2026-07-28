#include "drivers/w25qxx.h"
#include "spi.h"

/* ── Command set ──────────────────────────────────────────────────── */

#define CMD_WRITE_ENABLE  0x06
#define CMD_WRITE_DISABLE 0x04
#define CMD_READ_STATUS1  0x05
#define CMD_READ_DATA     0x03
#define CMD_PAGE_PROGRAM  0x02
#define CMD_SECTOR_ERASE  0x20
#define CMD_BLOCK_ERASE   0xD8
#define CMD_CHIP_ERASE    0xC7
#define CMD_JEDEC_ID      0x9F

#define STATUS_BUSY       0x01

/* ── Internal helpers ─────────────────────────────────────────────── */

static void cs_low(W25Qxx *flash)
{
    HAL_GPIO_WritePin(flash->cs_port, flash->cs_pin, GPIO_PIN_RESET);
}

static void cs_high(W25Qxx *flash)
{
    HAL_GPIO_WritePin(flash->cs_port, flash->cs_pin, GPIO_PIN_SET);
}

static uint8_t spi_tx(W25Qxx *flash, const uint8_t *data, uint32_t len)
{
    return HAL_SPI_Transmit(flash->hspi, (uint8_t *)data, len, 100 * len);
}

static uint8_t spi_rx(W25Qxx *flash, uint8_t *data, uint32_t len)
{
    return HAL_SPI_Receive(flash->hspi, data, len, 100 * len);
}

static uint8_t send_addr(W25Qxx *flash, uint8_t cmd, uint32_t addr)
{
    uint8_t buf[4];
    buf[0] = cmd;
    buf[1] = (uint8_t)(addr >> 16);
    buf[2] = (uint8_t)(addr >> 8);
    buf[3] = (uint8_t)(addr);
    return spi_tx(flash, buf, 4);
}

/* ── W25Qxx_Init ──────────────────────────────────────────────────── */

void W25Qxx_Init(W25Qxx *flash, SPI_HandleTypeDef *hspi,
                 GPIO_TypeDef *cs_port, uint16_t cs_pin)
{
    flash->hspi    = hspi;
    flash->cs_port = cs_port;
    flash->cs_pin  = cs_pin;
    cs_high(flash);
}

/* ── Read ──────────────────────────────────────────────────────────── */

uint8_t W25Qxx_Read(W25Qxx *flash, uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint8_t status;

    if (buf == NULL || len == 0) {
        return 1;
    }

    cs_low(flash);
    status = send_addr(flash, CMD_READ_DATA, addr);
    if (status == HAL_OK) {
        status = spi_rx(flash, buf, len);
    }
    cs_high(flash);
    return status;
}

uint8_t W25Qxx_Read_JEDEC_ID(W25Qxx *flash, uint8_t *id)
{
    uint8_t cmd = CMD_JEDEC_ID;
    uint8_t status;

    if (id == NULL) {
        return 1;
    }

    cs_low(flash);
    status = spi_tx(flash, &cmd, 1);
    if (status == HAL_OK) {
        status = spi_rx(flash, id, 3);
    }
    cs_high(flash);
    return status;
}

/* ── Status / Write-Enable ────────────────────────────────────────── */

uint8_t W25Qxx_WaitBusy(W25Qxx *flash)
{
    uint8_t sr;
    uint32_t timeout = 100000;

    do {
        cs_low(flash);
        uint8_t cmd = CMD_READ_STATUS1;
        spi_tx(flash, &cmd, 1);
        spi_rx(flash, &sr, 1);
        cs_high(flash);
        if (--timeout == 0) {
            return 2;
        };
    } while (sr & STATUS_BUSY);

    return HAL_OK;
}

uint8_t W25Qxx_WriteEnable(W25Qxx *flash)
{
    uint8_t cmd = CMD_WRITE_ENABLE;
    uint8_t status;

    cs_low(flash);
    status = spi_tx(flash, &cmd, 1);
    cs_high(flash);
    return status;
}

uint8_t W25Qxx_WriteDisable(W25Qxx *flash)
{
    uint8_t cmd = CMD_WRITE_DISABLE;
    uint8_t status;

    cs_low(flash);
    status = spi_tx(flash, &cmd, 1);
    cs_high(flash);
    return status;
}

/* ── Erase ────────────────────────────────────────────────────────── */

uint8_t W25Qxx_SectorErase(W25Qxx *flash, uint32_t addr)
{
    uint8_t status;
    status = W25Qxx_WriteEnable(flash);
    cs_low(flash);
    status |= send_addr(flash, CMD_SECTOR_ERASE, addr);
    cs_high(flash);
    status |= W25Qxx_WaitBusy(flash);
    return status;
}

uint8_t W25Qxx_BlockErase(W25Qxx *flash, uint32_t addr)
{
    uint8_t status;
    status = W25Qxx_WriteEnable(flash);
    cs_low(flash);
    status |= send_addr(flash, CMD_BLOCK_ERASE, addr);
    cs_high(flash);
    status |= W25Qxx_WaitBusy(flash);
    return status;
}

uint8_t W25Qxx_ChipErase(W25Qxx *flash)
{
    uint8_t cmd = CMD_CHIP_ERASE;
    uint8_t status;
    status = W25Qxx_WriteEnable(flash);
    cs_low(flash);
    status |= spi_tx(flash, &cmd, 1);
    cs_high(flash);
    status |= W25Qxx_WaitBusy(flash);
    return status;
}

/* ── Program ──────────────────────────────────────────────────────── */

uint8_t W25Qxx_PageProgram(W25Qxx *flash, uint32_t addr,
                           const uint8_t *buf, uint32_t len)
{
    uint8_t status;

    if (buf == NULL || len == 0) {
        return 1;
    }
    status = W25Qxx_WriteEnable(flash);
    cs_low(flash);
    status |= send_addr(flash, CMD_PAGE_PROGRAM, addr);
    if (status == HAL_OK) {
        status = spi_tx(flash, buf, len);
    }
    cs_high(flash);
    status |= W25Qxx_WaitBusy(flash);
    return status;
}

uint8_t W25Qxx_Write(W25Qxx *flash, uint32_t addr,
                     const uint8_t *buf, uint32_t len)
{
    uint32_t remain = len;
    uint32_t offset = 0;

    while (remain > 0) {
        uint32_t space = 256 - (addr & 0xFF); /* bytes left in page */
        uint32_t chunk = (remain < space) ? remain : space;

        if (W25Qxx_PageProgram(flash, addr, buf + offset, chunk) != HAL_OK) {
            return 1;
        }

        addr += chunk;
        offset += chunk;
        remain -= chunk;
    }

    return HAL_OK;
}
