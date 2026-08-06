/* SPDX-License-Identifier: Proprietary */
/* AegisOS — boot/arm64/context_switch.S
 * Save and restore task context for the scheduler.
 * void context_switch(struct task_context *prev, struct task_context *next);
 */

.section ".text", "ax"
.global context_switch

context_switch:
    /* Save callee-saved registers of prev task (x0 = prev ctx ptr) */
    stp x19, x20, [x0], #16
    stp x21, x22, [x0], #16
    stp x23, x24, [x0], #16
    stp x25, x26, [x0], #16
    stp x27, x28, [x0], #16
    stp x29, x30, [x0], #16
    mov x2, sp
    str x2,       [x0]

    /* Cooperative userspace scheduling happens from inside an EL0 SVC
     * exception. ELR_EL1, SPSR_EL1 and SP_EL0 are not part of the ordinary
     * GPR frame and are CPU-global, so preserve them per task as well.
     */
    mrs x2, elr_el1
    str x2, [x0, #8]
    mrs x2, spsr_el1
    str x2, [x0, #16]
    mrs x2, sp_el0
    str x2, [x0, #24]

    /* Restore callee-saved registers of next task (x1 = next ctx ptr) */
    ldp x19, x20, [x1], #16
    ldp x21, x22, [x1], #16
    ldp x23, x24, [x1], #16
    ldp x25, x26, [x1], #16
    ldp x27, x28, [x1], #16
    ldp x29, x30, [x1], #16
    ldr x2,       [x1]
    mov sp, x2

    ldr x2, [x1, #8]
    msr elr_el1, x2
    ldr x2, [x1, #16]
    msr spsr_el1, x2
    ldr x2, [x1, #24]
    msr sp_el0, x2
    isb

    ret
