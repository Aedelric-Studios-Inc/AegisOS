/* SPDX-License-Identifier: Proprietary */
/* AegisOS — boot/arm64/interrupts.S
 * Low-level IRQ/FIQ routing stubs.
 */

.section ".text", "ax"
.global irq_entry, fiq_entry

irq_entry:
    stp x0, x1, [sp, #-16]!
    bl  irq_dispatch
    ldp x0, x1, [sp], #16
    eret

fiq_entry:
    b .
