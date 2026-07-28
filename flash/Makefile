# Makefile – OpenOCD flash loader for SPI NOR Flash on STM32G031F8
# Usage:
#   make             build flash_loader.elf + flash_loader.bin
#   make clean        remove build artifacts

TC       = /opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi
CC       = $(TC)/bin/arm-none-eabi-gcc
OBJCOPY  = $(TC)/bin/arm-none-eabi-objcopy
OBJDUMP  = $(TC)/bin/arm-none-eabi-objdump
SIZE     = $(TC)/bin/arm-none-eabi-size

BUILD_DIR = build
TARGET    = $(BUILD_DIR)/flash_loader

CFLAGS   = -mcpu=cortex-m0plus         \
           -mthumb                     \
           -std=c11                    \
           -ffunction-sections         \
           -fdata-sections             \
           -fno-common                 \
           -Os                         \
           -Wall -Wextra               \
           -DSTM32G031xx               \
           -nostdlib                   \
           -ffreestanding              \
           -Wl,--gc-sections           \
           -Wl,-Map,$(TARGET).map      \
           -Wl,-Bstatic                \
           -Wl,--no-wchar-size-warning

LDFLAGS  = -T flash_loader.ld -nostartfiles -Wl,-e,Init \
           -Wl,--undefined=EraseSector \
           -Wl,--undefined=ProgramPage \
           -Wl,--undefined=Verify \
           -Wl,--undefined=EraseChip

OBJS     = $(BUILD_DIR)/flash_loader.o

.PHONY: all clean

all: $(BUILD_DIR) $(TARGET).elf $(TARGET).bin
	@$(SIZE) $(TARGET).elf
	@echo "=== Flash loader built ==="
	@echo "ELF: $(TARGET).elf"
	@echo "BIN: $(TARGET).bin"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/flash_loader.o: flash_loader.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR)
