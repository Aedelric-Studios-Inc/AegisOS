/* SPDX-License-Identifier: Proprietary */
/* AegisOS — boot/arm64/start.S
 * AArch64 reset entry point.
 */

.section ".text.boot", "ax"
.global _start
.extern _vectors

_start:
    /* Disable interrupts, set EL1 stack */
    msr     daifset, #0xf

    /* Zero BSS */
    adr     x0, __bss_start
    adr     x1, __bss_end
    cmp     x0, x1
    beq     1f
    mov     x2, #0
0:  str     x2, [x0], #8
    cmp     x0, x1
    blo     0b
1:
    /* Set up stack */
    adr     x0, _stack_top
    mov     sp, x0

    /* Install exception vector table */
    adr     x0, _vectors
    msr     vbar_el1, x0
    isb

    /* Jump to kernel main */
    bl      kernel_main

    /* Should never return */
halt:
    wfe
    b       halt
