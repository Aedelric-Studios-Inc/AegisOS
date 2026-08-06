/* SPDX-License-Identifier: Proprietary */
/* AegisOS — boot/arm64/atomics.S
 * AArch64 atomic operations (load-acquire / store-release).
 */

.section ".text", "ax"
.global atomic_load_64, atomic_store_64, atomic_add_64, atomic_cas_64

/* uint64_t atomic_load_64(uint64_t *addr) */
atomic_load_64:
    ldar    x0, [x0]
    ret

/* void atomic_store_64(uint64_t *addr, uint64_t val) */
atomic_store_64:
    stlr    x1, [x0]
    ret

/* uint64_t atomic_add_64(uint64_t *addr, uint64_t val) — returns old value */
atomic_add_64:
0:  ldaxr   x2, [x0]
    add     x3, x2, x1
    stlxr   w4, x3, [x0]
    cbnz    w4, 0b
    mov     x0, x2
    ret

/* uint64_t atomic_cas_64(uint64_t *addr, uint64_t expected, uint64_t desired)
 * Returns old value. */
atomic_cas_64:
0:  ldaxr   x3, [x0]
    cmp     x3, x1
    bne     1f
    stlxr   w4, x2, [x0]
    cbnz    w4, 0b
1:  mov     x0, x3
    ret
