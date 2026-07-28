# SPI NOR Flash Loader – 使用示例

## 快速开始

```bash
# 1. 编译 flash loader
make -C flash

# 2. 启动 OpenOCD (ST-Link + STM32G031)
openocd -f interface/stlink.cfg \
        -f target/stm32g0x.cfg \
        -f flash/openocd_w25qxx.cfg
```

## 典型场景

### 场景 A — 擦除 + 写入 + 校验 VL53L8CX 固件

```bash
# 终端 1: 启动 OpenOCD server
openocd -f interface/stlink.cfg -f target/stm32g0x.cfg -f flash/openocd_w25qxx.cfg

# 终端 2: telnet 连接并执行
telnet localhost 4444
> reset halt
> flash erase_sector 0 0 63          # 擦除前 64 个 sector (64 × 4KB = 256KB)
> flash write_image vl53l8cx_fw.bin 0x90000000
> flash verify_image vl53l8cx_fw.bin 0x90000000
> reset run
```

### 场景 B — 单命令写入 + 校验

```bash
# 终端 2
> reset halt
> program vl53l8cx_fw.bin 0x90000000 verify   # erase + write + verify 一条龙
> reset run
```

### 场景 C — 全片擦除 + 重新烧录

```bash
> reset halt
> flash erase_sector 0 0 255                 # 全片擦除 (256 sectors × 4KB = 1MB)
> flash write_image vl53l8cx_fw.bin 0x90000000
> verify_image vl53l8cx_fw.bin 0x90000000
> reset run
```

### 场景 D — 只擦除不写入 (清理固件区域)

```bash
> reset halt
> flash erase_sector 0 0 20                  # 擦除前 84KB (VL53L8CX FW 区域)
> reset run
```

## 地址映射

OpenOCD 中的 flash bank 基地址 `0x90000000` 映射到 SPI Flash 物理地址 `0x000000`：

| 用途 | SPI Flash 物理地址 | OpenOCD 地址 | 大小 |
|------|-------------------|-------------|------|
| VL53L8CX firmware | `0x000000` | `0x90000000` | ~84 KB |
| 用户数据区 | `0x015000` | `0x90015000` | 剩余空间 |

> **提示**: `0x90000000` 只是 OpenOCD 内部使用的虚拟地址，与硬件无关。所有偏移计算在此基础上。

## 常见芯片容量 & sector 数

| 型号 | 容量 | Sector 数 (4KB) | 全片擦除命令 |
|------|------|----------------|-------------|
| W25Q80 / GD25Q80 | 1 MB | 0..255 | `flash erase_sector 0 0 255` |
| W25Q16 / GD25Q16 | 2 MB | 0..511 | `flash erase_sector 0 0 511` |
| W25Q32 / GD25Q32 | 4 MB | 0..1023 | `flash erase_sector 0 0 1023` |

## 只擦除 VL53L8CX 固件区域 (安全，不影响其他数据)

```bash
# 84KB = roundup(84/4) = 21 sectors  (0..20)
> flash erase_sector 0 0 20
```

## 读取 SPI Flash 内容到文件

```bash
# 读取前 128KB 备份
> reset halt
> dump_image spi_backup.bin 0x90000000 0x20000
```

## 一键脚本示例

```bash
#!/bin/bash
# flash_vl53l8cx.sh – 烧录 VL53L8CX 固件到外部 SPI Flash
set -e

FW_BIN="${1:-vl53l8cx_fw.bin}"
OOCD_CFG_DIR="/home/luo/Project/Horse/TipSensor/FirmWare"

if [ ! -f "$FW_BIN" ]; then
    echo "Usage: $0 <firmware.bin>"
    exit 1
fi

openocd -f interface/stlink.cfg \
        -f target/stm32g0x.cfg \
        -f flash/openocd_w25qxx.cfg \
        -c "init" \
        -c "reset halt" \
        -c "flash erase_sector 0 0 20" \
        -c "flash write_image $FW_BIN 0x90000000" \
        -c "flash verify_image $FW_BIN 0x90000000" \
        -c "reset run" \
        -c "exit"

echo "=== Flash done: $FW_BIN ==="
```

## 故障排查

| 症状 | 可能原因 | 解决方法 |
|------|---------|---------|
| `Error: flash loader missing` | loader 路径不对 | 确认 `openocd_w25qxx.cfg` 中路径为 `flash/build/flash_loader.elf` |
| `Error: init failed` | SPI Flash 硬件连接问题 | 检查 PB6(CS)、PA5/6/7(SCK/MISO/MOSI) 焊接和接线 |
| `verify failed` | 写入数据不一致 | 先全片擦除再写入，检查 SPI 速率是否过高 |
| `Cannot access target` | SWD 连接问题 | 检查 ST-Link 连接，确认 MCU 上电 |
| `no flash bank found` | 配置未加载 | 确保 `openocd_w25qxx.cfg` 在 init 之前 source |
