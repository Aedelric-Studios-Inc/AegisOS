/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/memory/page_tables.c
 * AArch64 4-level page table management.
 */

#include "memory.h"

/* Page table entry flags */
#define PTE_VALID       (1ULL << 0)
#define PTE_TABLE       (1ULL << 1)
#define PTE_PAGE        (1ULL << 1)
#define PTE_AF          (1ULL << 10)  /* Access flag */
#define PTE_SH_INNER    (3ULL << 8)
#define PTE_MAIR_IDX(n) ((u64)(n) << 2)
#define PTE_AP_RW_EL1   (0ULL << 6)
#define PTE_AP_RW_ALL   (1ULL << 6)

#define PTE_KERNEL_RW   (PTE_VALID | PTE_PAGE | PTE_AF | PTE_SH_INNER | PTE_MAIR_IDX(0) | PTE_AP_RW_EL1)
#define PTE_USER_RW     (PTE_VALID | PTE_PAGE | PTE_AF | PTE_SH_INNER | PTE_MAIR_IDX(0) | PTE_AP_RW_ALL)

#define L0_SHIFT  39
#define L1_SHIFT  30
#define L2_SHIFT  21
#define L3_SHIFT  12
#define INDEX_MASK 0x1FFULL

static u64 alloc_page_table(void) {
    phys_addr_t pa = phys_alloc_page();
    if (!pa) return 0;
    /* Zero the page */
    u64 *p = (u64 *)pa;
    for (int i = 0; i < 512; i++) p[i] = 0;
    return pa;
}

int page_table_init(void) {
    extern u64 _kernel_pgtable[];
    for (int i = 0; i < 512; i++) _kernel_pgtable[i] = 0;
    return AEGIS_OK;
}

int page_table_map(u64 *l0, virt_addr_t va, phys_addr_t pa, u32 flags) {
    u64 l0_idx = (va >> L0_SHIFT) & INDEX_MASK;
    u64 l1_idx = (va >> L1_SHIFT) & INDEX_MASK;
    u64 l2_idx = (va >> L2_SHIFT) & INDEX_MASK;
    u64 l3_idx = (va >> L3_SHIFT) & INDEX_MASK;

    /* L0 -> L1 */
    if (!(l0[l0_idx] & PTE_VALID)) {
        u64 new_l1 = alloc_page_table();
        if (!new_l1) return AEGIS_ENOMEM;
        l0[l0_idx] = new_l1 | PTE_VALID | PTE_TABLE;
    }
    u64 *l1 = (u64 *)(l0[l0_idx] & ~0xFFFULL);

    /* L1 -> L2 */
    if (!(l1[l1_idx] & PTE_VALID)) {
        u64 new_l2 = alloc_page_table();
        if (!new_l2) return AEGIS_ENOMEM;
        l1[l1_idx] = new_l2 | PTE_VALID | PTE_TABLE;
    }
    u64 *l2 = (u64 *)(l1[l1_idx] & ~0xFFFULL);

    /* L2 -> L3 */
    if (!(l2[l2_idx] & PTE_VALID)) {
        u64 new_l3 = alloc_page_table();
        if (!new_l3) return AEGIS_ENOMEM;
        l2[l2_idx] = new_l3 | PTE_VALID | PTE_TABLE;
    }
    u64 *l3 = (u64 *)(l2[l2_idx] & ~0xFFFULL);

    /* L3 entry: physical page */
    (void)flags;
    l3[l3_idx] = (pa & ~0xFFFULL) | PTE_KERNEL_RW;

    return AEGIS_OK;
}
