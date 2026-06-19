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
    extern u64 _kernel_pgtable[];
    u64 offset = 0;
    while (offset < size) {
        virt_addr_t cur = va + offset;
        /* Walk L0 -> L1 -> L2 -> L3 and clear the L3 entry */
        u64 l0_idx = (cur >> 39) & 0x1FFULL;
        if (!(_kernel_pgtable[l0_idx] & 1ULL)) { offset += 4096; continue; }
        u64 *l1 = (u64 *)(_kernel_pgtable[l0_idx] & ~0xFFFULL);

        u64 l1_idx = (cur >> 30) & 0x1FFULL;
        if (!(l1[l1_idx] & 1ULL)) { offset += 4096; continue; }
        u64 *l2 = (u64 *)(l1[l1_idx] & ~0xFFFULL);

        u64 l2_idx = (cur >> 21) & 0x1FFULL;
        if (!(l2[l2_idx] & 1ULL)) { offset += 4096; continue; }
        u64 *l3 = (u64 *)(l2[l2_idx] & ~0xFFFULL);

        u64 l3_idx = (cur >> 12) & 0x1FFULL;
        l3[l3_idx] = 0;

        offset += 4096;
    }

    /* TLB invalidation: flush the entire TLB after unmapping */
    __asm__ volatile(
        "dsb ishst\n"
        "tlbi vmalle1is\n"
        "dsb ish\n"
        "isb\n"
        ::: "memory"
    );

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
