/* SPDX-License-Identifier: Proprietary */
#pragma once
/* memory.h — physical and virtual memory management interface */

#include "types.h"

#define PAGE_SIZE       4096UL
#define PAGE_SHIFT      12
#define PAGE_MASK       (~(PAGE_SIZE - 1))

#define PAGE_ALIGN(x)   (((uptr)(x) + PAGE_SIZE - 1) & PAGE_MASK)
#define PAGE_ALIGN_DOWN(x) ((uptr)(x) & PAGE_MASK)

/* Physical memory */
int  phys_mem_init(phys_addr_t base, u64 size);
phys_addr_t phys_alloc_page(void);
void phys_free_page(phys_addr_t addr);
u64  phys_mem_free_pages(void);

/* Virtual memory */
int  virt_mem_init(void);
int  virt_map(virt_addr_t va, phys_addr_t pa, u64 size, u32 flags);
int  virt_unmap(virt_addr_t va, u64 size);
phys_addr_t virt_to_phys(virt_addr_t va);

/* Heap */
void  heap_init(virt_addr_t base, u64 size);
void *kmalloc(u64 size);
void  kfree(void *ptr);
void *krealloc(void *ptr, u64 new_size);

/* Page tables */
int  page_table_init(void);
int  page_table_map(u64 *table, virt_addr_t va, phys_addr_t pa, u32 flags);
