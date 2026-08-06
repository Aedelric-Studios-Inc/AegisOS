/* SPDX-License-Identifier: Proprietary */
/* AegisOS — boot/arm64/exceptions.S
 * AArch64 exception handlers.
 */

.section ".text", "ax"
.global el1_sync_sp0, el1_irq_sp0, el1_fiq_sp0, el1_error_sp0
.global el1_sync_spx, el1_irq_spx, el1_fiq_spx, el1_error_spx
.global el0_sync_a64, el0_irq_a64, el0_fiq_a64, el0_error_a64
.global el0_sync_a32, el0_irq_a32, el0_fiq_a32, el0_error_a32

.macro save_regs
    stp x0,  x1,  [sp, #-16]!
    stp x2,  x3,  [sp, #-16]!
    stp x4,  x5,  [sp, #-16]!
    stp x6,  x7,  [sp, #-16]!
    stp x8,  x9,  [sp, #-16]!
    stp x10, x11, [sp, #-16]!
    stp x12, x13, [sp, #-16]!
    stp x14, x15, [sp, #-16]!
    stp x16, x17, [sp, #-16]!
    stp x18, x19, [sp, #-16]!
    stp x20, x21, [sp, #-16]!
    stp x22, x23, [sp, #-16]!
    stp x24, x25, [sp, #-16]!
    stp x26, x27, [sp, #-16]!
    stp x28, x29, [sp, #-16]!
    str x30, [sp, #-8]!
.endm

.macro restore_regs
    ldr x30, [sp], #8
    ldp x28, x29, [sp], #16
    ldp x26, x27, [sp], #16
    ldp x24, x25, [sp], #16
    ldp x22, x23, [sp], #16
    ldp x20, x21, [sp], #16
    ldp x18, x19, [sp], #16
    ldp x16, x17, [sp], #16
    ldp x14, x15, [sp], #16
    ldp x12, x13, [sp], #16
    ldp x10, x11, [sp], #16
    ldp x8,  x9,  [sp], #16
    ldp x6,  x7,  [sp], #16
    ldp x4,  x5,  [sp], #16
    ldp x2,  x3,  [sp], #16
    ldp x0,  x1,  [sp], #16
.endm

el1_sync_sp0:
el1_irq_sp0:
el1_fiq_sp0:
el1_error_sp0:
el1_sync_spx:
    save_regs
    mov x0, sp
    bl  handle_exception
    restore_regs
    eret

el1_irq_spx:
    save_regs
    mov x0, sp
    bl  irq_dispatch
    restore_regs
    eret

el1_fiq_spx:
el1_error_spx:
    b .

el0_sync_a64:
    save_regs
    mov x0, sp
    bl  syscall_handler
    restore_regs
    eret

el0_irq_a64:
    save_regs
    mov x0, sp
    bl  irq_dispatch
    restore_regs
    eret

el0_fiq_a64:
el0_error_a64:
el0_sync_a32:
el0_irq_a32:
el0_fiq_a32:
el0_error_a32:
    b .
