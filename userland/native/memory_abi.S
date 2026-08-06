/* SPDX-License-Identifier: Proprietary */
/*
 * Freestanding C memory ABI for native AegisOS AArch64 userland.
 *
 * LLVM may emit calls to these symbols even for no_std Rust programs.  AegisOS
 * does not link a hosted libc, so every native Rust ELF links this object.
 * Byte-at-a-time loops are deliberate: they are safe before SIMD/FP enablement,
 * tolerate unaligned addresses, and cannot be rewritten recursively by LLVM.
 */

.section .text, "ax"
.align 4

.global memset
.type memset, %function
memset:
    mov     x3, x0
    cbz     x2, 2f
1:
    strb    w1, [x3], #1
    subs    x2, x2, #1
    b.ne    1b
2:
    ret
.size memset, . - memset

.global memcpy
.type memcpy, %function
memcpy:
    mov     x3, x0
    cbz     x2, 2f
1:
    ldrb    w4, [x1], #1
    strb    w4, [x3], #1
    subs    x2, x2, #1
    b.ne    1b
2:
    ret
.size memcpy, . - memcpy

.global memmove
.type memmove, %function
memmove:
    cbz     x2, 4f
    cmp     x0, x1
    b.eq    4f
    b.lo    1f

    /* Destination follows source: copy backwards to preserve overlap. */
    add     x3, x0, x2
    add     x4, x1, x2
2:
    ldrb    w5, [x4, #-1]!
    strb    w5, [x3, #-1]!
    subs    x2, x2, #1
    b.ne    2b
    b       4f

    /* Destination precedes source: a forward copy is overlap-safe. */
1:
    mov     x3, x0
3:
    ldrb    w5, [x1], #1
    strb    w5, [x3], #1
    subs    x2, x2, #1
    b.ne    3b
4:
    ret
.size memmove, . - memmove

.global memcmp
.type memcmp, %function
memcmp:
    cbz     x2, 3f
1:
    ldrb    w3, [x0], #1
    ldrb    w4, [x1], #1
    cmp     w3, w4
    b.ne    2f
    subs    x2, x2, #1
    b.ne    1b
3:
    mov     w0, wzr
    ret
2:
    sub     w0, w3, w4
    ret
.size memcmp, . - memcmp

.global bcmp
.type bcmp, %function
bcmp:
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp
    bl      memcmp
    cmp     w0, #0
    cset    w0, ne
    ldp     x29, x30, [sp], #16
    ret
.size bcmp, . - bcmp

.global strlen
.type strlen, %function
strlen:
    mov     x1, x0
    mov     x0, #0
1:
    ldrb    w2, [x1, x0]
    cbz     w2, 2f
    add     x0, x0, #1
    b       1b
2:
    ret
.size strlen, . - strlen

.section .note.GNU-stack, "", %progbits
