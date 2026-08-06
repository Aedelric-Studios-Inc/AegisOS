/* SPDX-License-Identifier: Proprietary */
OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

PHDRS {
    text PT_LOAD FLAGS(5); /* PF_R | PF_X */
    data PT_LOAD FLAGS(6); /* PF_R | PF_W */
}

SECTIONS {
    . = 0x0000000000400000;
    .text : ALIGN(4096) {
        KEEP(*(.text.entry))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4096);
    } :text

    .data : ALIGN(4096) {
        *(.data*)
        *(.got*)
        *(.sdata*)
    } :data
    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        __bss_end = .;
        . = ALIGN(4096);
    } :data

    /DISCARD/ : { *(.comment*) *(.note*) *(.eh_frame*) *(.gnu*) }
}
