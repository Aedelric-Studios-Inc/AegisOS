/* SPDX-License-Identifier: Proprietary */
/* AegisOS — boot/arm64/syscall_entry.S
 * Syscall entry: save caller-saved registers, dispatch, restore.
 * Convention: x8 = syscall number, x0-x5 = args, x0 = return.
 */

.section ".text", "ax"
.global syscall_entry

syscall_entry:
    /* Save registers */
    stp x0,  x1,  [sp, #-16]!
    stp x2,  x3,  [sp, #-16]!
    stp x4,  x5,  [sp, #-16]!
    stp x6,  x7,  [sp, #-16]!
    stp x8,  x30, [sp, #-16]!

    /* Dispatch through C table */
    bl  syscall_dispatch

    /* Restore and return */
    ldp x8,  x30, [sp], #16
    ldp x6,  x7,  [sp], #16
    ldp x4,  x5,  [sp], #16
    ldp x2,  x3,  [sp], #16
    ldp x0,  x1,  [sp], #16
    eret
