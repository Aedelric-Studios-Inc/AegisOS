/* SPDX-License-Identifier: Proprietary */
/* AegisOS — boot/arm64/tlb.S
 * TLB maintenance operations.
 */

.section ".text", "ax"
.global tlb_flush_all, tlb_flush_va

tlb_flush_all:
    dsb     ishst
    tlbi    vmalle1is
    dsb     ish
    isb
    ret

tlb_flush_va:
    /* x0 = virtual address */
    lsr     x0, x0, #12
    dsb     ishst
    tlbi    vaae1is, x0
    dsb     ish
    isb
    ret
