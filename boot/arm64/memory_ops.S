/* SPDX-License-Identifier: Proprietary */
/* AegisOS — boot/arm64/memory_ops.S
 * Alignment-safe bulk memory operations and the freestanding C ABI entry
 * points Clang is allowed to emit even for -ffreestanding builds.
 */

.section ".text", "ax"
.global aegis_memcpy, aegis_memset, aegis_memzero
.global memcpy, memset, memmove, memcmp, bcmp, strlen

/* void *memcpy(void *dst, const void *src, size_t n) */
/* void *aegis_memcpy(void *dst, const void *src, size_t n) */
memcpy:
aegis_memcpy:
    mov     x3, x0
    cbz     x2, 1f
0:  ldrb    w4, [x1], #1
    strb    w4, [x3], #1
    subs    x2, x2, #1
    b.ne    0b
1:  ret

/* void *memset(void *dst, int c, size_t n) */
/* void *aegis_memset(void *dst, int c, size_t n) */
memset:
aegis_memset:
    mov     x3, x0
    cbz     x2, 1f
    and     w1, w1, #0xff
0:  strb    w1, [x3], #1
    subs    x2, x2, #1
    b.ne    0b
1:  ret

/* void aegis_memzero(void *dst, size_t n) */
aegis_memzero:
    cbz     x1, 1f
0:  strb    wzr, [x0], #1
    subs    x1, x1, #1
    b.ne    0b
1:  ret

/* void *memmove(void *dst, const void *src, size_t n) */
memmove:
    mov     x3, x0
    cbz     x2, 3f
    cmp     x0, x1
    b.eq    3f
    b.lo    1f
    add     x4, x1, x2
    cmp     x0, x4
    b.hs    1f

    /* Overlap with dst above src: copy backwards. */
    add     x4, x1, x2
    add     x5, x0, x2
0:  ldrb    w6, [x4, #-1]!
    strb    w6, [x5, #-1]!
    subs    x2, x2, #1
    b.ne    0b
    b       3f

    /* No destructive overlap: copy forwards. */
1:  ldrb    w4, [x1], #1
    strb    w4, [x3], #1
    subs    x2, x2, #1
    b.ne    1b
3:  ret

/* int memcmp(const void *a, const void *b, size_t n) */
memcmp:
    cbz     x2, 2f
0:  ldrb    w3, [x0], #1
    ldrb    w4, [x1], #1
    cmp     w3, w4
    b.ne    1f
    subs    x2, x2, #1
    b.ne    0b
2:  mov     w0, wzr
    ret
1:  sub     w0, w3, w4
    ret

/* int bcmp(const void *a, const void *b, size_t n) */
bcmp:
    b       memcmp

/* size_t strlen(const char *s) */
strlen:
    mov     x1, x0
0:  ldrb    w2, [x1], #1
    cbnz    w2, 0b
    sub     x0, x1, x0
    sub     x0, x0, #1
    ret
