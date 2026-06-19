/* SPDX-License-Identifier: Proprietary */
/* AegisOS AArch64 kernel linker script */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

KERNEL_BASE = 0xFFFFFF8000000000;
LOAD_BASE   = 0x40080000;

SECTIONS {
    . = LOAD_BASE;

    .text.boot : {
        KEEP(*(.text.boot))
    }

    .text.vectors : ALIGN(2048) {
        KEEP(*(.text.vectors))
    }

    .text : ALIGN(4096) {
        *(.text*)
    }

    .rodata : ALIGN(4096) {
        *(.rodata*)
    }

    .data : ALIGN(4096) {
        *(.data*)
    }

    .bss : ALIGN(4096) {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(4096);
    _stack_bottom = .;
    . += 0x10000;  /* 64 KB kernel stack */
    _stack_top = .;

    . = ALIGN(4096);
    _kernel_pgtable = .;
    . += 0x5000;   /* 5 pages for initial page tables */

    _end = .;
}
