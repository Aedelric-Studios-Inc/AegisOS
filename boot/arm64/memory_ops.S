/* SPDX-License-Identifier: Proprietary */
/* AegisOS — boot/arm64/memory_ops.S
 * Optimized bulk memory operations.
 */

.section ".text", "ax"
.global aegis_memcpy, aegis_memset, aegis_memzero

/* void *aegis_memcpy(void *dst, const void *src, size_t n) */
aegis_memcpy:
    mov     x3, x0
    cbz     x2, 1f
0:  ldrb    w4, [x1], #1
    strb    w4, [x3], #1
    subs    x2, x2, #1
    bne     0b
1:  ret

/* void *aegis_memset(void *dst, int c, size_t n) */
aegis_memset:
    mov     x3, x0
    cbz     x2, 1f
    and     w1, w1, #0xff
0:  strb    w1, [x3], #1
    subs    x2, x2, #1
    bne     0b
1:  ret

/* void aegis_memzero(void *dst, size_t n) */
aegis_memzero:
    cbz     x1, 1f
0:  strb    wzr, [x0], #1
    subs    x1, x1, #1
    bne     0b
1:  ret
