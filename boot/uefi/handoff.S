/* SPDX-License-Identifier: Proprietary */
/* AArch64 UEFI-to-AegisOS handoff.
 * x0 = kernel entry, x1 = FDT, x2 = loaded region base, x3 = region size.
 */
.text
.p2align 2
.globl aegis_uefi_handoff
aegis_uefi_handoff:
    msr     daifset, #0xf
    mov     x9, x0
    mov     x10, x1

    /* Publish loader writes before instruction fetch from the kernel image. */
    mov     x11, x2
    add     x12, x2, x3
1:
    cmp     x11, x12
    b.hs    2f
    dc      cvau, x11
    add     x11, x11, #64
    b       1b
2:
    dsb     ish
    mov     x11, x2
3:
    cmp     x11, x12
    b.hs    4f
    ic      ivau, x11
    add     x11, x11, #64
    b       3b
4:
    dsb     ish
    isb

    mrs     x11, CurrentEL
    cmp     x11, #0x8
    b.eq    .Lfrom_el2
    cmp     x11, #0x4
    b.eq    .Lfrom_el1
.Lhang:
    wfe
    b       .Lhang

.Lfrom_el1:
    /* Required AArch64 boot state: EL1, MMU/caches off, interrupts masked. */
    movz    x11, #0x0800
    movk    x11, #0x30d0, lsl #16
    dsb     sy
    msr     sctlr_el1, x11
    isb
    tlbi    vmalle1
    dsb     sy
    isb
    mov     x0, x10
    br      x9

.Lfrom_el2:
    /* Configure a clean AArch64 EL1h execution environment. */
    movz    x11, #0x8000, lsl #16
    msr     hcr_el2, x11
    mov     x11, #3
    msr     cnthctl_el2, x11
    msr     cntvoff_el2, xzr
    msr     cptr_el2, xzr
    msr     vbar_el1, xzr
    msr     ttbr0_el1, xzr
    msr     ttbr1_el1, xzr
    movz    x11, #0x0800
    movk    x11, #0x30d0, lsl #16
    msr     sctlr_el1, x11

    msr     elr_el2, x9
    mov     x11, #0x3c5
    msr     spsr_el2, x11
    mov     x0, x10

    mrs     x11, sctlr_el2
    bic     x11, x11, #1
    bic     x11, x11, #4
    bic     x11, x11, #0x1000
    dsb     sy
    msr     sctlr_el2, x11
    isb
    tlbi    alle2
    dsb     sy
    isb
    eret
