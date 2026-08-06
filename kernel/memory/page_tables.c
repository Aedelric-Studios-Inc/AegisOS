/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/memory/page_tables.c
 * AArch64 4-level page table management.
 *
 * v45 extends the early mapper so page entries carry real EL0/EL1 access
 * attributes.  The kernel still boots through the proven early table, while
 * v45 prepares bounded per-process TTBR0 roots for user/kernel isolation.
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
#define PTE_AP_RO_EL1   (2ULL << 6)
#define PTE_AP_RO_ALL   (3ULL << 6)
#define PTE_AP_MASK     (3ULL << 6)
#define PTE_PXN         (1ULL << 53)
#define PTE_UXN         (1ULL << 54)
#define PTE_ADDR_MASK   0x0000FFFFFFFFF000ULL

#define L0_SHIFT  39
#define L1_SHIFT  30
#define L2_SHIFT  21
#define L3_SHIFT  12
#define INDEX_MASK 0x1FFULL

static u64 alloc_page_table(void) {
    phys_addr_t pa = phys_alloc_page();
    if (!pa) return 0;
    /* Zero the page.  Early AegisOS identity maps the QEMU RAM window. */
    u64 *p = (u64 *)(uptr)pa;
    for (int i = 0; i < 512; i++) p[i] = 0;
    return pa;
}

static u64 page_attrs_from_flags(u32 flags) {
    u64 attrs = PTE_VALID | PTE_PAGE | PTE_AF | PTE_SH_INNER | PTE_MAIR_IDX(0);

    if (flags & AEGIS_VM_USER) {
        if (flags & AEGIS_VM_WRITE) attrs |= PTE_AP_RW_ALL;
        else attrs |= PTE_AP_RO_ALL;

        /* User executable text is executable at EL0 but never at EL1. */
        attrs |= PTE_PXN;
        if ((flags & AEGIS_VM_EXEC) == 0) attrs |= PTE_UXN;
    } else {
        if (flags & AEGIS_VM_WRITE) attrs |= PTE_AP_RW_EL1;
        else attrs |= PTE_AP_RO_EL1;

        /* Default kernel mappings are not user-accessible. */
        if ((flags & AEGIS_VM_EXEC) == 0) attrs |= PTE_UXN | PTE_PXN;
        else attrs |= PTE_UXN;
    }

    return attrs;
}

int page_table_init(void) {
    extern u64 _kernel_pgtable[];
    for (int i = 0; i < 512; i++) _kernel_pgtable[i] = 0;
    return AEGIS_OK;
}

int page_table_map(u64 *l0, virt_addr_t va, phys_addr_t pa, u32 flags) {
    if (!l0) return AEGIS_EINVAL;
    if ((va & (PAGE_SIZE - 1ULL)) != 0 || (pa & (PAGE_SIZE - 1ULL)) != 0) {
        return AEGIS_EINVAL;
    }
    if (flags & AEGIS_VM_GUARD) return AEGIS_EINVAL;

    u64 l0_idx = (va >> L0_SHIFT) & INDEX_MASK;
    u64 l1_idx = (va >> L1_SHIFT) & INDEX_MASK;
    u64 l2_idx = (va >> L2_SHIFT) & INDEX_MASK;
    u64 l3_idx = (va >> L3_SHIFT) & INDEX_MASK;

    /* L0 -> L1 */
    if (!(l0[l0_idx] & PTE_VALID)) {
        u64 new_l1 = alloc_page_table();
        if (!new_l1) return AEGIS_ENOMEM;
        l0[l0_idx] = (new_l1 & PTE_ADDR_MASK) | PTE_VALID | PTE_TABLE;
    }
    u64 *l1 = (u64 *)(uptr)(l0[l0_idx] & PTE_ADDR_MASK);

    /* L1 -> L2 */
    if (!(l1[l1_idx] & PTE_VALID)) {
        u64 new_l2 = alloc_page_table();
        if (!new_l2) return AEGIS_ENOMEM;
        l1[l1_idx] = (new_l2 & PTE_ADDR_MASK) | PTE_VALID | PTE_TABLE;
    }
    u64 *l2 = (u64 *)(uptr)(l1[l1_idx] & PTE_ADDR_MASK);

    /* L2 -> L3 */
    if (!(l2[l2_idx] & PTE_VALID)) {
        u64 new_l3 = alloc_page_table();
        if (!new_l3) return AEGIS_ENOMEM;
        l2[l2_idx] = (new_l3 & PTE_ADDR_MASK) | PTE_VALID | PTE_TABLE;
    }
    u64 *l3 = (u64 *)(uptr)(l2[l2_idx] & PTE_ADDR_MASK);

    /* L3 entry: physical page with requested EL0/EL1 permissions. */
    l3[l3_idx] = (pa & PTE_ADDR_MASK) | page_attrs_from_flags(flags);

    return AEGIS_OK;
}

int page_table_query(u64 *l0, virt_addr_t va, phys_addr_t *pa_out, u64 *entry_out) {
    if (!l0) return AEGIS_EINVAL;

    u64 l0_idx = (va >> L0_SHIFT) & INDEX_MASK;
    u64 l1_idx = (va >> L1_SHIFT) & INDEX_MASK;
    u64 l2_idx = (va >> L2_SHIFT) & INDEX_MASK;
    u64 l3_idx = (va >> L3_SHIFT) & INDEX_MASK;

    if (!(l0[l0_idx] & PTE_VALID)) return AEGIS_ENOENT;
    u64 *l1 = (u64 *)(uptr)(l0[l0_idx] & PTE_ADDR_MASK);

    if (!(l1[l1_idx] & PTE_VALID)) return AEGIS_ENOENT;
    u64 *l2 = (u64 *)(uptr)(l1[l1_idx] & PTE_ADDR_MASK);

    if (!(l2[l2_idx] & PTE_VALID)) return AEGIS_ENOENT;
    u64 *l3 = (u64 *)(uptr)(l2[l2_idx] & PTE_ADDR_MASK);

    u64 entry = l3[l3_idx];
    if (!(entry & PTE_VALID)) return AEGIS_ENOENT;

    if (pa_out) *pa_out = (phys_addr_t)((entry & PTE_ADDR_MASK) | (va & (PAGE_SIZE - 1ULL)));
    if (entry_out) *entry_out = entry;
    return AEGIS_OK;
}

bool page_table_entry_is_valid(u64 entry) {
    return (entry & PTE_VALID) != 0;
}

bool page_table_entry_allows_user(u64 entry) {
    u64 ap = entry & PTE_AP_MASK;
    return ap == PTE_AP_RW_ALL || ap == PTE_AP_RO_ALL;
}

bool page_table_entry_allows_write(u64 entry) {
    u64 ap = entry & PTE_AP_MASK;
    return ap == PTE_AP_RW_EL1 || ap == PTE_AP_RW_ALL;
}

bool page_table_entry_allows_el0_execute(u64 entry) {
    return page_table_entry_allows_user(entry) && ((entry & PTE_UXN) == 0);
}

bool page_table_entry_is_privileged_only(u64 entry) {
    return page_table_entry_is_valid(entry) && !page_table_entry_allows_user(entry);
}
