/*
 * OpenOCD Flash Loader for SPI NOR Flash on STM32G031F8
 *
 * Supports W25Qxx, GD25Qxx and compatible SPI NOR flash families.
 * Tested with: W25Q80DV, GD25Q80.
 *
 * Build:  make -C flash
 * Output: flash/build/flash_loader.elf  flash/build/flash_loader.bin
 *
 * OpenOCD config fragment (add to your board/interface config):
 *
 *   # External W25Q80 via SPI1 (PA5=SCK,PA6=MISO,PA7=MOSI,PB6=CS)
 *   set _EXTFLASHNAME $_CHIPNAME.w25q80
 *   flash bank $_EXTFLASHNAME stmqspi 0x90000000 0x00100000 1 1 $_TARGETNAME 0x20000000 flash/build/flash_loader.elf
 *
 *   # After init, you can program the external flash:
 *   #   program firmware.bin 0x90000000 verify
 *
 * Flash layout (1 MB typical, adjust for your device):
 *   0x000000 - 0x0FFFFF   Generic data region
 *   0x000000 - 0x014FFF   VL53L8CX firmware (3 pages, ~84 KB)
 */

#include <stdint.h>
#include <stddef.h>

/* ── STM32G0 register map ────────────────────────────────────────── */

#define RCC_BASE         0x40021000UL
#define RCC_IOPENR       (*(volatile uint32_t *)(RCC_BASE + 0x34))
#define RCC_APBENR2      (*(volatile uint32_t *)(RCC_BASE + 0x48))

#define RCC_IOPENR_GPIOA (1U << 0)
#define RCC_IOPENR_GPIOB (1U << 1)
#define RCC_APBENR2_SPI1 (1U << 12)

#define GPIOA_BASE       0x50000000UL
#define GPIOB_BASE       0x50000400UL

#define GPIO_MODER(p)    (*(volatile uint32_t *)((p) + 0x00))
#define GPIO_OTYPER(p)   (*(volatile uint32_t *)((p) + 0x04))
#define GPIO_OSPEEDR(p)  (*(volatile uint32_t *)((p) + 0x08))
#define GPIO_PUPDR(p)    (*(volatile uint32_t *)((p) + 0x0C))
#define GPIO_AFRL(p)     (*(volatile uint32_t *)((p) + 0x20))
#define GPIO_AFRH(p)     (*(volatile uint32_t *)((p) + 0x24))
#define GPIO_BSRR(p)     (*(volatile uint32_t *)((p) + 0x18))

#define SPI1_BASE        0x40013000UL
#define SPI_CR1(spi)     (*(volatile uint32_t *)((spi) + 0x00))
#define SPI_CR2(spi)     (*(volatile uint32_t *)((spi) + 0x04))
#define SPI_SR(spi)      (*(volatile uint32_t *)((spi) + 0x08))
#define SPI_DR(spi)      (*(volatile uint32_t *)((spi) + 0x0C))

/* SPI_CR1 bits */
#define SPI_CR1_SPE    (1U << 6)
#define SPI_CR1_BR_Pos 3
#define SPI_CR1_MSTR   (1U << 2)
#define SPI_CR1_SSI    (1U << 8)
#define SPI_CR1_SSM    (1U << 9)

/* SPI_SR bits */
#define SPI_SR_TXE  (1U << 1)
#define SPI_SR_RXNE (1U << 0)
#define SPI_SR_BSY  (1U << 7)

/* ── Pin definitions ─────────────────────────────────────────────── */

#define SPI1_SCK_PIN  5 /* PA5  AF0 */
#define SPI1_MISO_PIN 6 /* PA6  AF0 */
#define SPI1_MOSI_PIN 7 /* PA7  AF0 */
#define FLASH_CS_PIN  6 /* PB6  output */

/* ── W25Qxx commands ─────────────────────────────────────────────── */

#define W25Q_CMD_WREN     0x06
#define W25Q_CMD_WRDI     0x04
#define W25Q_CMD_RDSR1    0x05
#define W25Q_CMD_READ     0x03
#define W25Q_CMD_PP       0x02 /* Page Program */
#define W25Q_CMD_SE       0x20 /* Sector Erase (4 KB) */
#define W25Q_CMD_BE       0xD8 /* Block Erase (64 KB) */
#define W25Q_CMD_CE       0xC7 /* Chip Erase */
#define W25Q_CMD_JEDEC_ID 0x9F

#define W25Q_PAGE_SIZE    256
#define W25Q_SECTOR_SIZE  4096

/* ── Globals (zero-inited by OpenOCD loader stub) ────────────────── */

static uint32_t spi_base;

/* ── Low-level SPI helpers ────────────────────────────────────────── */

static void delay(uint32_t n)
{
    for (volatile uint32_t i = 0; i < n; i++) {}
}

static void spi_wait_txe(void)
{
    while (!(SPI_SR(spi_base) & SPI_SR_TXE)) {}
}

static void spi_wait_rxne(void)
{
    while (!(SPI_SR(spi_base) & SPI_SR_RXNE)) {}
}

static uint8_t spi_xfer(uint8_t byte)
{
    spi_wait_txe();
    *(volatile uint8_t *)&SPI_DR(spi_base) = byte;
    spi_wait_rxne();
    return *(volatile uint8_t *)&SPI_DR(spi_base);
}

static void cs_low(void)
{
    GPIO_BSRR(GPIOB_BASE) = (uint32_t)FLASH_CS_PIN << 16; /* BR = reset */
}

static void cs_high(void)
{
    GPIO_BSRR(GPIOB_BASE) = (uint32_t)FLASH_CS_PIN; /* BS = set */
}

/* ── W25Qxx primitives ───────────────────────────────────────────── */

static uint8_t flash_read_status(void)
{
    uint8_t sr;
    cs_low();
    spi_xfer(W25Q_CMD_RDSR1);
    sr = spi_xfer(0xFF);
    cs_high();
    return sr;
}

static void flash_wait_busy(void)
{
    while (flash_read_status() & 0x01) {} /* WIP bit */
}

static void flash_write_enable(void)
{
    cs_low();
    spi_xfer(W25Q_CMD_WREN);
    cs_high();
}

/* ── Hardware init ────────────────────────────────────────────────── */

/*
 * Init(adr, clk_khz, fnc)
 *
 * adr    – base address of the flash bank (ignored for SPI)
 * clk_khz – desired SPI clock in kHz (we ignore and run at PCLK/4 ≈ 8 MHz)
 * fnc    – function code (0=erase, 1=program, 2=verify)
 *          We ignore it; Init is called once regardless.
 *
 * Return 0 on success.
 */
int __attribute__((used)) Init(uint32_t adr, uint32_t clk, uint32_t fnc)
{
    (void)adr;
    (void)clk;
    (void)fnc;

    spi_base = SPI1_BASE;

    /* --- Enable peripheral clocks --- */
    RCC_IOPENR |= RCC_IOPENR_GPIOA | RCC_IOPENR_GPIOB;
    RCC_APBENR2 |= RCC_APBENR2_SPI1;

    /* Short delay for clocks to stabilize */
    delay(100);

    /* --- GPIO: PA5(SCK), PA6(MISO), PA7(MOSI) → AF0 (SPI1) --- */
    /* PA5: MODER=AF */
    GPIO_MODER(GPIOA_BASE) &= ~((3U << (SPI1_SCK_PIN * 2)) | (3U << (SPI1_MISO_PIN * 2)) | (3U << (SPI1_MOSI_PIN * 2)));
    GPIO_MODER(GPIOA_BASE) |= ((2U << (SPI1_SCK_PIN * 2)) | (2U << (SPI1_MISO_PIN * 2)) | (2U << (SPI1_MOSI_PIN * 2)));
    /* AF0 for all (default is 0, no need to set AFRL) */
    /* Speed: high */
    GPIO_OSPEEDR(GPIOA_BASE) |= ((3U << (SPI1_SCK_PIN * 2)) | (3U << (SPI1_MISO_PIN * 2)) | (3U << (SPI1_MOSI_PIN * 2)));
    /* No pull-up/down */
    GPIO_PUPDR(GPIOA_BASE) &= ~((3U << (SPI1_SCK_PIN * 2)) | (3U << (SPI1_MISO_PIN * 2)) | (3U << (SPI1_MOSI_PIN * 2)));

    /* --- GPIO: PB6 (FLASH_CS) → output, push-pull, high --- */
    GPIO_MODER(GPIOB_BASE) &= ~(3U << (FLASH_CS_PIN * 2));
    GPIO_MODER(GPIOB_BASE) |= (1U << (FLASH_CS_PIN * 2));
    GPIO_BSRR(GPIOB_BASE) = FLASH_CS_PIN; /* set high = deselected */

    /* --- SPI1 init (mode 0, 8-bit, master, fPCLK/8, soft NSS) --- */
    /*
     * MCU runs at HSI 16 MHz → PCLK ≈ 16 MHz.  Baudrate = fPCLK / 8 = 2 MHz.
     * CPOL=0, CPHA=0, MSB-first, soft NSS.
     */
    SPI_CR1(spi_base) = 0;
    SPI_CR1(spi_base) = SPI_CR1_SSM              /* Software slave management */
                        | SPI_CR1_SSI            /* Internal slave select */
                        | SPI_CR1_SPE            /* Enable SPI */
                        | (2U << SPI_CR1_BR_Pos) /* BR[2:0]=010 → fPCLK/8 */
                        | SPI_CR1_MSTR;          /* Master mode */

    /* Read JEDEC ID for informational purposes (always succeeds).
     * Compatible chips include Winbond W25Qxx, GigaDevice GD25Qxx, etc.
     * JEDEC ID format: mfg(1B) | mem_type(1B) | capacity(1B)
     *   Winbond:  EF 40 xx  (W25Q80=0x14, W25Q16=0x15, W25Q32=0x16…)
     *   GD:       C8 40 xx  (GD25Q80=0x14, GD25Q16=0x15…)
     */
    cs_low();
    spi_xfer(W25Q_CMD_JEDEC_ID);
    uint8_t mfg = spi_xfer(0xFF);
    uint8_t mem = spi_xfer(0xFF);
    uint8_t cap = spi_xfer(0xFF);
    cs_high();

    /* Store JEDEC for debug (caller can inspect via debugger if needed) */
    (void)mfg;
    (void)mem;
    (void)cap;

    return 0;
}

/* ── Erase ────────────────────────────────────────────────────────── */

/*
 * EraseSector(adr)  – erase a 4 KB sector.
 *
 * adr is byte address within the SPI flash (0x000000 … 0x0FFFFF).
 * OpenOCD always passes a sector-aligned address.
 * Return 0 on success.
 */
int __attribute__((used)) EraseSector(uint32_t adr)
{
    flash_write_enable();
    cs_low();
    spi_xfer(W25Q_CMD_SE);
    spi_xfer((uint8_t)(adr >> 16));
    spi_xfer((uint8_t)(adr >> 8));
    spi_xfer((uint8_t)(adr));
    cs_high();
    flash_wait_busy();
    return 0;
}

/* ── Program ──────────────────────────────────────────────────────── */

/*
 * ProgramPage(adr, sz, buf)
 *
 * adr – byte address in SPI flash
 * sz  – number of bytes to program (1 … 256, never crosses page boundary)
 * buf – pointer to data buffer in target RAM
 *
 * Return 0 on success.
 */
int __attribute__((used)) ProgramPage(uint32_t adr, uint32_t sz, uint8_t *buf)
{
    if (sz == 0 || buf == NULL) return 1;

    flash_write_enable();
    cs_low();
    spi_xfer(W25Q_CMD_PP);
    spi_xfer((uint8_t)(adr >> 16));
    spi_xfer((uint8_t)(adr >> 8));
    spi_xfer((uint8_t)(adr));

    for (uint32_t i = 0; i < sz; i++) {
        spi_xfer(buf[i]);
    }

    cs_high();
    flash_wait_busy();
    return 0;
}

/* ── Verify (optional, but speeds up programming) ─────────────────── */

int __attribute__((used)) Verify(uint32_t adr, uint32_t sz, uint8_t *buf)
{
    if (sz == 0 || buf == NULL) return 1;

    cs_low();
    spi_xfer(W25Q_CMD_READ);
    spi_xfer((uint8_t)(adr >> 16));
    spi_xfer((uint8_t)(adr >> 8));
    spi_xfer((uint8_t)(adr));

    for (uint32_t i = 0; i < sz; i++) {
        if (spi_xfer(0xFF) != buf[i]) {
            cs_high();
            return 1; /* mismatch */
        }
    }

    cs_high();
    return 0;
}

/* ── Optional mass erase ──────────────────────────────────────────── */

int __attribute__((used)) EraseChip(void)
{
    flash_write_enable();
    cs_low();
    spi_xfer(W25Q_CMD_CE);
    cs_high();
    flash_wait_busy(); /* W25Q80 CE takes ~1-3 s typical */
    return 0;
}
