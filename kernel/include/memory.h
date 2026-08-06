/* SPDX-License-Identifier: Proprietary */
#pragma once
/* memory.h — physical and virtual memory management interface */

#include "types.h"

#define PAGE_SIZE       4096UL
#define PAGE_SHIFT      12
#define PAGE_MASK       (~(PAGE_SIZE - 1))

#define PAGE_ALIGN(x)   (((uptr)(x) + PAGE_SIZE - 1) & PAGE_MASK)
#define PAGE_ALIGN_DOWN(x) ((uptr)(x) & PAGE_MASK)

/* Generic page-table mapping permission flags. */
#define AEGIS_VM_READ      (1u << 0)
#define AEGIS_VM_WRITE     (1u << 1)
#define AEGIS_VM_EXEC      (1u << 2)
#define AEGIS_VM_USER      (1u << 3)
#define AEGIS_VM_KERNEL    (1u << 4)
#define AEGIS_VM_GUARD     (1u << 5)

/* Physical memory */
int  phys_mem_init(phys_addr_t base, u64 size);
phys_addr_t phys_alloc_page(void);
void phys_free_page(phys_addr_t addr);
u64  phys_mem_free_pages(void);
u64  phys_mem_total_pages(void);
u64  phys_mem_used_pages(void);
int  phys_reserve_range(phys_addr_t base, u64 size);
bool phys_addr_is_reserved(phys_addr_t addr);

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
int  page_table_query(u64 *table, virt_addr_t va, phys_addr_t *pa_out, u64 *entry_out);
bool page_table_entry_is_valid(u64 entry);
bool page_table_entry_allows_user(u64 entry);
bool page_table_entry_allows_write(u64 entry);
bool page_table_entry_allows_el0_execute(u64 entry);
bool page_table_entry_is_privileged_only(u64 entry);
