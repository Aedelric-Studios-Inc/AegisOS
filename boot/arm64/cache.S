/* SPDX-License-Identifier: Proprietary */
/* AegisOS — boot/arm64/cache.S
 * Cache maintenance operations.
 */

.section ".text", "ax"
.global cache_flush_all, dcache_clean_range, icache_invalidate_all

cache_flush_all:
    dsb     sy
    /* Clean and invalidate all data caches by set/way would go here */
    isb
    ret

dcache_clean_range:
    /* x0 = start, x1 = end */
    mrs     x2, ctr_el0
    ubfx    x2, x2, #16, #4
    mov     x3, #4
    lsl     x3, x3, x2          /* cache line size */
    sub     x4, x3, #1
    bic     x0, x0, x4
0:  dc      civac, x0
    add     x0, x0, x3
    cmp     x0, x1
    blo     0b
    dsb     sy
    ret

icache_invalidate_all:
    ic      iallu
    dsb     sy
    isb
    ret
