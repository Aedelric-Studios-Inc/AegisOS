/* SPDX-License-Identifier: Proprietary */
/* AegisOS — boot/arm64/el0_transition.S
 * v31.0 controlled EL0 transition probe + v32 first tiny process body +
 * v33 syscall return-path continuation proof + v34 tiny process exit proof.
 *
 * This is not the full user process launcher.  It deliberately performs one
 * minimal eret from EL1 into a tiny EL0 stub, then uses SVC to trap through the
 * existing EL0 synchronous exception vector.  v33 proves the kernel can return
 * to the instruction after SVC and that EL0 continues executing.  v34 then
 * issues SYS_EXIT and proves the kernel marks the user process dead without
 * returning to that dead EL0 context.
 */

.section ".text", "ax"
.global aegis_el0_transition_enter
.global aegis_el0_transition_test_entry
.global aegis_first_user_process_entry
.global aegis_first_user_process_after_syscall
.global aegis_first_user_process_return_probe
.global aegis_first_user_process_exit_call
.global aegis_first_user_process_after_exit

/* void aegis_el0_transition_enter(u64 entry, u64 user_sp)
 * x0 = EL0 entry PC
 * x1 = EL0 stack pointer
 * Does not return.  The EL0 stub traps back through SVC.
 */
aegis_el0_transition_enter:
    msr sp_el0, x1
    msr elr_el1, x0

    /* Return to EL0t with DAIF masked.  M[3:0] = 0000b = EL0t. */
    mov x2, #0x3c0
    msr spsr_el1, x2
    isb
    eret

/* Tiny EL0 probe body.
 * If the eret above succeeds, the trace will show execution here.  The SVC
 * should vector back to el0_sync_a64, where the kernel marks the test as seen.
 */
aegis_el0_transition_test_entry:
aegis_first_user_process_entry:
    /* v32/v33 tiny aegis-init user body.  First syscall proves EL0 can call
     * the kernel.  v33 requires the kernel to place the return value into x0
     * and eret back to the instruction after SVC.
     */
    mov x0, #0x32
    movk x0, #0x3200, lsl #16
    mov x8, #5              /* SYS_GETPID */
    svc #0

/* Reaching this label proves the syscall return path restored EL0 context and
 * resumed execution after SVC.  v34 immediately issues SYS_EXIT so the kernel
 * can prove process-exit accounting and deliberately stop instead of returning
 * to a dead EL0 context.
 */
aegis_first_user_process_after_syscall:
    mov x9, x0              /* Preserve SYS_GETPID return value for trace/debug. */
aegis_first_user_process_return_probe:
aegis_first_user_process_exit_call:
    mov x0, #0x34
    movk x0, #0x3400, lsl #16
    mov x8, #4              /* SYS_EXIT */
    svc #0

/* If this ever executes, the kernel returned to a dead user process.  The v34
 * proof script intentionally does not require this symbol to be reached.
 */
aegis_first_user_process_after_exit:
1:
    wfe
    b 1b
