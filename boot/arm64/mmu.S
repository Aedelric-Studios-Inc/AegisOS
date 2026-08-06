/* SPDX-License-Identifier: Proprietary */
/* AegisOS — boot/arm64/mmu.S
 * Early MMU and translation table setup for AArch64.
 */

.section ".text.boot", "ax"
.global mmu_init

mmu_init:
    /* Invalidate all TLB entries */
    tlbi    vmalle1is
    dsb     ish
    isb

    /* Set MAIR_EL1: index 0 = normal cacheable, index 1 = device nGnRnE */
    ldr     x0, =0x00ff
    msr     mair_el1, x0

    /* Set TCR_EL1 */
    ldr     x0, =0x00000001B5193519
    msr     tcr_el1, x0

    /* Load translation table base */
    ldr     x0, =_kernel_pgtable
    msr     ttbr1_el1, x0
    msr     ttbr0_el1, x0

    isb

    /* Enable MMU in SCTLR_EL1 */
    mrs     x0, sctlr_el1
    orr     x0, x0, #(1 << 0)   /* M: MMU enable */
    orr     x0, x0, #(1 << 2)   /* C: data cache enable */
    orr     x0, x0, #(1 << 12)  /* I: instruction cache enable */
    msr     sctlr_el1, x0
    isb

    ret
