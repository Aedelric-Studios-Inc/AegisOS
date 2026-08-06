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

static u64 page_index_for_addr(phys_addr_t addr) {
    if (addr < phys_base) return total_pages;
    return (addr - phys_base) / PAGE_SIZE;
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

u64 phys_mem_total_pages(void) {
    return total_pages;
}

u64 phys_mem_used_pages(void) {
    return total_pages - free_pages_count;
}

int phys_reserve_range(phys_addr_t base, u64 size) {
    if (size == 0) return AEGIS_OK;
    if (total_pages == 0) return AEGIS_EINVAL;

    phys_addr_t start = (phys_addr_t)PAGE_ALIGN_DOWN(base);
    phys_addr_t end = (phys_addr_t)PAGE_ALIGN(base + size);

    if (end <= phys_base) return AEGIS_EINVAL;
    if (start < phys_base) start = phys_base;

    u64 first = page_index_for_addr(start);
    u64 last = page_index_for_addr(end - 1);
    if (first >= total_pages) return AEGIS_EINVAL;
    if (last >= total_pages) last = total_pages - 1;

    for (u64 i = first; i <= last; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            if (free_pages_count > 0) free_pages_count--;
        }
    }
    return AEGIS_OK;
}

bool phys_addr_is_reserved(phys_addr_t addr) {
    u64 page = page_index_for_addr(addr);
    if (page >= total_pages) return false;
    return bitmap_test(page);
}
