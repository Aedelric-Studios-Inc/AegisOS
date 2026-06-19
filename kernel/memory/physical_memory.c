/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/memory/physical_memory.c
 * Physical page frame allocator (bitmap-based).
 */

#include "memory.h"
#include "panic.h"

#define MAX_PAGES (1024 * 1024)   /* support up to 4 GB */

static u8  bitmap[MAX_PAGES / 8];
static u64 phys_base;
static u64 total_pages;
static u64 free_pages_count;

static void bitmap_set(u64 page) {
    bitmap[page / 8] |= (u8)(1 << (page % 8));
}

static void bitmap_clear(u64 page) {
    bitmap[page / 8] &= (u8)~(1 << (page % 8));
}

static bool bitmap_test(u64 page) {
    return (bitmap[page / 8] >> (page % 8)) & 1;
}

int phys_mem_init(phys_addr_t base, u64 size) {
    phys_base        = base;
    total_pages      = size / PAGE_SIZE;
    free_pages_count = total_pages;

    if (total_pages > MAX_PAGES) PANIC("too much RAM for bitmap allocator");

    /* Mark all pages as free */
    for (u64 i = 0; i < total_pages / 8; i++) bitmap[i] = 0;

    return AEGIS_OK;
}

phys_addr_t phys_alloc_page(void) {
    for (u64 i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages_count--;
            return phys_base + i * PAGE_SIZE;
        }
    }
    return (phys_addr_t)0;  /* Out of memory */
}

void phys_free_page(phys_addr_t addr) {
    u64 page = (addr - phys_base) / PAGE_SIZE;
    if (page >= total_pages) return;
    bitmap_clear(page);
    free_pages_count++;
}

u64 phys_mem_free_pages(void) {
    return free_pages_count;
}
