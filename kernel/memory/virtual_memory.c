/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/memory/virtual_memory.c
 * Virtual memory management.
 */

#include "memory.h"
#include "panic.h"

int virt_mem_init(void) {
    return page_table_init();
}

int virt_map(virt_addr_t va, phys_addr_t pa, u64 size, u32 flags) {
    u64 offset = 0;
    while (offset < size) {
        extern u64 _kernel_pgtable[];
        int ret = page_table_map(_kernel_pgtable, va + offset, pa + offset, flags);
        if (ret != AEGIS_OK) return ret;
        offset += PAGE_SIZE;
    }
    return AEGIS_OK;
}

int virt_unmap(virt_addr_t va, u64 size) {
    (void)va; (void)size;
    /* TLB shootdown and page table walk unmap — TODO */
    return AEGIS_OK;
}

phys_addr_t virt_to_phys(virt_addr_t va) {
    /* Use AT S1E1R instruction for hardware address translation */
    u64 par;
    __asm__ volatile(
        "at s1e1r, %1\n"
        "mrs %0, par_el1\n"
        : "=r"(par) : "r"(va) : "memory"
    );
    if (par & 1) return (phys_addr_t)0;  /* Translation fault */
    return (phys_addr_t)((par & 0x0000FFFFFFFFF000ULL) | (va & 0xFFF));
}
