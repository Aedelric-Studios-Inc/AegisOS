/* SPDX-License-Identifier: Proprietary */
#pragma once
/* AegisOS v45 real user/kernel page-table isolation proof layer.
 *
 * v45 scope: allocate real per-user-process TTBR0 roots, map bounded EL0
 * regions with EL0 PTE permissions, leave guard pages unmapped, verify the
 * kernel window is not visible through user roots, and preserve the already
 * proven v44 supervisor + EL0 launch/exit chain.
 */

#include "types.h"

#define AEGIS_PAGE_ISOLATION_PROCESS_MAX 8U
#define AEGIS_PAGE_ISOLATION_NAME_MAX    32U

#define AEGIS_PAGE_ISOLATION_TEXT_PAGES   2U
#define AEGIS_PAGE_ISOLATION_HEAP_PAGES   2U
#define AEGIS_PAGE_ISOLATION_IPC_PAGES    1U
#define AEGIS_PAGE_ISOLATION_STACK_PAGES  1U

typedef struct aegis_user_address_space {
    u32 pid;
    u32 service_id;
    char name[AEGIS_PAGE_ISOLATION_NAME_MAX];
    phys_addr_t ttbr0_root;
    u32 mapped_text_pages;
    u32 mapped_heap_pages;
    u32 mapped_ipc_pages;
    u32 mapped_stack_pages;
    bool root_allocated;
    bool user_text_rx;
    bool user_heap_rw_nx;
    bool user_ipc_rw_nx;
    bool user_stack_rw_nx;
    bool guard_unmapped;
    bool kernel_window_blocked;
    bool ttbr0_contract_ready;
} aegis_user_address_space_t;

typedef struct aegis_page_isolation_state {
    bool initialised;
    bool descriptor_roots_ready;
    bool user_regions_mapped;
    bool guard_pages_unmapped;
    bool kernel_window_denied;
    bool ttbr0_switch_contract_ready;
    bool supervisor_fault_boundary_preserved;
    bool isolation_ready;
    u32 address_space_count;
    u32 page_table_root_count;
    u32 mapped_user_pages;
    u32 text_rx_pages;
    u32 writable_nx_pages;
    u32 guard_page_count;
    u32 kernel_denied_checks;
    phys_addr_t first_ttbr0_root;
    phys_addr_t last_ttbr0_root;
} aegis_page_isolation_state_t;

void page_isolation_init(void);
int  page_isolation_prepare_user_kernel_tables(void);
int  page_isolation_selftest(void);

const aegis_page_isolation_state_t *page_isolation_state(void);
const aegis_user_address_space_t *page_isolation_address_space(u32 index);
bool page_isolation_ready(void);
bool page_isolation_kernel_window_denied(void);
bool page_isolation_guard_pages_unmapped(void);
bool page_isolation_ttbr0_contract_ready(void);
