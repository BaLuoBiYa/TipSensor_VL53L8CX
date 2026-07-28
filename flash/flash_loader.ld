/*
 * Linker script for OpenOCD SPI Flash loader (STM32G0xx)
 *
 * Loader runs entirely from RAM. OpenOCD loads the binary at the
 * work-area base (0x20000000) and jumps to Init.
 *
 * RAM: 8 KB (0x20000000 - 0x20002000)
 *   .text + .rodata  :  0x20000000
 *   .data + .bss     :  after .text
 *   stack            :  0x20002000 (top of RAM)
 */

MEMORY
{
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 8K
}

ENTRY(Init)

SECTIONS
{
    .text :
    {
        KEEP(*(.vectors))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > RAM

    .data :
    {
        *(.data*)
        . = ALIGN(4);
    } > RAM

    .bss (NOLOAD) :
    {
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
    } > RAM

    /* OpenOCD uses the last 32 bytes for stack + stub data.
       Reserve them from the .stack section. */
    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        _sstack = .;
        . = . + 0x200;          /* 512 B stack */
        . = ALIGN(8);
        _estack = .;
    } > RAM
}
