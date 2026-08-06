/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/memory/page_isolation.c
 * v45 real user/kernel page-table isolation proof layer.
 */

#include "page_isolation.h"
#include "memory.h"
#include "userland.h"
#include "service_supervisor.h"

extern char _start[];
extern char _end[];
extern char _stack_bottom[];
extern char _stack_top[];

static aegis_user_address_space_t spaces[AEGIS_PAGE_ISOLATION_PROCESS_MAX];
static aegis_page_isolation_state_t isolation_state;

static void iso_zero(void *ptr, u64 len) {
    u8 *p = (u8 *)ptr;
    for (u64 i = 0; i < len; i++) p[i] = 0;
}

static void iso_copy_name(char dst[AEGIS_PAGE_ISOLATION_NAME_MAX], const char *src) {
    u32 i = 0;
    if (!src) src = "unnamed";
    while (src[i] && i < AEGIS_PAGE_ISOLATION_NAME_MAX - 1U) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static phys_addr_t iso_alloc_zero_page(void) {
    phys_addr_t pa = phys_alloc_page();
    if (!pa) return 0;
    u8 *p = (u8 *)(uptr)pa;
    for (u64 i = 0; i < PAGE_SIZE; i++) p[i] = 0;
    return pa;
}

static int iso_alloc_root(aegis_user_address_space_t *as) {
    if (!as) return AEGIS_EINVAL;
    phys_addr_t root = iso_alloc_zero_page();
    if (!root) return AEGIS_ENOMEM;
    if ((root & (PAGE_SIZE - 1ULL)) != 0) return AEGIS_EINVAL;
    as->ttbr0_root = root;
    as->root_allocated = true;
    isolation_state.page_table_root_count++;
    if (isolation_state.first_ttbr0_root == 0) isolation_state.first_ttbr0_root = root;
    isolation_state.last_ttbr0_root = root;
    return AEGIS_OK;
}

static int iso_map_page(aegis_user_address_space_t *as, virt_addr_t va, u32 vm_flags) {
    if (!as || !as->root_allocated) return AEGIS_EINVAL;
    if ((va & (PAGE_SIZE - 1ULL)) != 0) return AEGIS_EINVAL;

    phys_addr_t backing = iso_alloc_zero_page();
    if (!backing) return AEGIS_ENOMEM;

    int rc = page_table_map((u64 *)(uptr)as->ttbr0_root, va, backing, vm_flags | AEGIS_VM_USER);
    if (rc != AEGIS_OK) return rc;
    isolation_state.mapped_user_pages++;
    return AEGIS_OK;
}

static int iso_query_entry(const aegis_user_address_space_t *as, virt_addr_t va, u64 *entry_out) {
    if (!as || !as->root_allocated) return AEGIS_EINVAL;
    return page_table_query((u64 *)(uptr)as->ttbr0_root, va, 0, entry_out);
}

static int iso_verify_text_rx(aegis_user_address_space_t *as, const aegis_user_process_descriptor_t *proc) {
    for (u32 i = 0; i < AEGIS_PAGE_ISOLATION_TEXT_PAGES; i++) {
        virt_addr_t va = proc->text.base + ((virt_addr_t)i * PAGE_SIZE);
        int rc = iso_map_page(as, va, AEGIS_VM_READ | AEGIS_VM_EXEC);
        if (rc != AEGIS_OK) return rc;
        as->mapped_text_pages++;

        u64 entry = 0;
        rc = iso_query_entry(as, va, &entry);
        if (rc != AEGIS_OK) return rc;
        if (!page_table_entry_allows_user(entry)) return AEGIS_EPERM;
        if (!page_table_entry_allows_el0_execute(entry)) return AEGIS_EPERM;
        if (page_table_entry_allows_write(entry)) return AEGIS_EPERM;
        isolation_state.text_rx_pages++;
    }
    as->user_text_rx = true;
    return AEGIS_OK;
}

static int iso_verify_rw_nx_page(aegis_user_address_space_t *as, virt_addr_t va, u32 *mapped_count) {
    int rc = iso_map_page(as, va, AEGIS_VM_READ | AEGIS_VM_WRITE);
    if (rc != AEGIS_OK) return rc;
    (*mapped_count)++;

    u64 entry = 0;
    rc = iso_query_entry(as, va, &entry);
    if (rc != AEGIS_OK) return rc;
    if (!page_table_entry_allows_user(entry)) return AEGIS_EPERM;
    if (!page_table_entry_allows_write(entry)) return AEGIS_EPERM;
    if (page_table_entry_allows_el0_execute(entry)) return AEGIS_EPERM;
    isolation_state.writable_nx_pages++;
    return AEGIS_OK;
}

static int iso_verify_heap_rw_nx(aegis_user_address_space_t *as, const aegis_user_process_descriptor_t *proc) {
    for (u32 i = 0; i < AEGIS_PAGE_ISOLATION_HEAP_PAGES; i++) {
        int rc = iso_verify_rw_nx_page(as, proc->heap.base + ((virt_addr_t)i * PAGE_SIZE), &as->mapped_heap_pages);
        if (rc != AEGIS_OK) return rc;
    }
    as->user_heap_rw_nx = true;
    return AEGIS_OK;
}

static int iso_verify_ipc_rw_nx(aegis_user_address_space_t *as, const aegis_user_process_descriptor_t *proc) {
    for (u32 i = 0; i < AEGIS_PAGE_ISOLATION_IPC_PAGES; i++) {
        int rc = iso_verify_rw_nx_page(as, proc->ipc.base + ((virt_addr_t)i * PAGE_SIZE), &as->mapped_ipc_pages);
        if (rc != AEGIS_OK) return rc;
    }
    as->user_ipc_rw_nx = true;
    return AEGIS_OK;
}

static int iso_verify_stack_rw_nx(aegis_user_address_space_t *as, const aegis_user_process_descriptor_t *proc) {
    /* Map the top page below SP, not the guard page below the stack. */
    virt_addr_t top_page = proc->stack.base + proc->stack.size - PAGE_SIZE;
    for (u32 i = 0; i < AEGIS_PAGE_ISOLATION_STACK_PAGES; i++) {
        int rc = iso_verify_rw_nx_page(as, top_page - ((virt_addr_t)i * PAGE_SIZE), &as->mapped_stack_pages);
        if (rc != AEGIS_OK) return rc;
    }
    as->user_stack_rw_nx = true;
    return AEGIS_OK;
}

static int iso_verify_guard_unmapped(aegis_user_address_space_t *as, const aegis_user_process_descriptor_t *proc) {
    u64 entry = 0;
    int rc = iso_query_entry(as, proc->stack_guard.base, &entry);
    if (rc != AEGIS_ENOENT) return AEGIS_EPERM;
    (void)entry;
    as->guard_unmapped = true;
    isolation_state.guard_page_count++;
    return AEGIS_OK;
}

static int iso_verify_kernel_window_denied(aegis_user_address_space_t *as) {
    const virt_addr_t probes[] = {
        (virt_addr_t)(uptr)_start,
        (virt_addr_t)(uptr)_end,
        (virt_addr_t)(uptr)_stack_bottom,
        (virt_addr_t)(uptr)_stack_top,
        0xffffff8000000000ULL,
    };

    for (u32 i = 0; i < (u32)(sizeof(probes) / sizeof(probes[0])); i++) {
        u64 entry = 0;
        int rc = iso_query_entry(as, probes[i], &entry);
        if (rc == AEGIS_OK && page_table_entry_allows_user(entry)) return AEGIS_EPERM;
        if (rc == AEGIS_OK && !page_table_entry_is_privileged_only(entry)) return AEGIS_EPERM;
        if (rc != AEGIS_OK && rc != AEGIS_ENOENT) return rc;
        isolation_state.kernel_denied_checks++;
    }

    as->kernel_window_blocked = true;
    return AEGIS_OK;
}

static int iso_prepare_process_space(u32 index, const aegis_user_process_descriptor_t *proc) {
    if (index >= AEGIS_PAGE_ISOLATION_PROCESS_MAX) return AEGIS_ENOMEM;
    if (!proc || proc->id == 0 || proc->service_id == 0) return AEGIS_EINVAL;
    if ((proc->entry & 0x3ULL) != 0) return AEGIS_EINVAL;
    if ((proc->initial_sp & 0xFULL) != 0) return AEGIS_EINVAL;

    aegis_user_address_space_t *as = &spaces[index];
    iso_zero(as, sizeof(*as));
    as->pid = proc->id;
    as->service_id = proc->service_id;
    iso_copy_name(as->name, proc->name);

    int rc = iso_alloc_root(as);
    if (rc != AEGIS_OK) return rc;
    rc = iso_verify_text_rx(as, proc);
    if (rc != AEGIS_OK) return rc;
    rc = iso_verify_heap_rw_nx(as, proc);
    if (rc != AEGIS_OK) return rc;
    rc = iso_verify_ipc_rw_nx(as, proc);
    if (rc != AEGIS_OK) return rc;
    rc = iso_verify_stack_rw_nx(as, proc);
    if (rc != AEGIS_OK) return rc;
    rc = iso_verify_guard_unmapped(as, proc);
    if (rc != AEGIS_OK) return rc;
    rc = iso_verify_kernel_window_denied(as);
    if (rc != AEGIS_OK) return rc;

    u64 entry_pte = 0;
    rc = iso_query_entry(as, proc->entry, &entry_pte);
    if (rc != AEGIS_OK) return rc;
    u64 sp_pte = 0;
    rc = iso_query_entry(as, PAGE_ALIGN_DOWN(proc->initial_sp), &sp_pte);
    if (rc != AEGIS_OK) return rc;
    if (!page_table_entry_allows_el0_execute(entry_pte)) return AEGIS_EPERM;
    if (!page_table_entry_allows_user(sp_pte) || !page_table_entry_allows_write(sp_pte)) return AEGIS_EPERM;

    as->ttbr0_contract_ready = true;
    return AEGIS_OK;
}

void page_isolation_init(void) {
    iso_zero(spaces, sizeof(spaces));
    iso_zero(&isolation_state, sizeof(isolation_state));
    isolation_state.initialised = true;
}

int page_isolation_prepare_user_kernel_tables(void) {
    if (!isolation_state.initialised) return AEGIS_EINVAL;
    if (!userland_real_multiprocess_launch_ready()) return AEGIS_EINVAL;
    if (!userland_page_permissions_ready()) return AEGIS_EINVAL;
    if (!service_supervisor_handoff_preserved()) return AEGIS_EINVAL;

    u32 count = userland_process_descriptor_count();
    if (count < 4U) return AEGIS_EINVAL;
    if (count > AEGIS_PAGE_ISOLATION_PROCESS_MAX) count = AEGIS_PAGE_ISOLATION_PROCESS_MAX;

    for (u32 i = 0; i < count; i++) {
        const aegis_user_process_descriptor_t *proc = userland_process_descriptor(i);
        int rc = iso_prepare_process_space(i, proc);
        if (rc != AEGIS_OK) return rc;
        isolation_state.address_space_count++;
    }

    isolation_state.descriptor_roots_ready = (isolation_state.page_table_root_count == count);
    isolation_state.user_regions_mapped = (isolation_state.mapped_user_pages >= count * 6U);
    isolation_state.guard_pages_unmapped = (isolation_state.guard_page_count == count);
    isolation_state.kernel_window_denied = (isolation_state.kernel_denied_checks >= count * 5U);

    u32 contracts = 0;
    for (u32 i = 0; i < count; i++) {
        if (spaces[i].ttbr0_contract_ready) contracts++;
    }
    isolation_state.ttbr0_switch_contract_ready = (contracts == count);
    isolation_state.supervisor_fault_boundary_preserved = service_supervisor_handoff_preserved();
    isolation_state.isolation_ready = isolation_state.descriptor_roots_ready &&
                                      isolation_state.user_regions_mapped &&
                                      isolation_state.guard_pages_unmapped &&
                                      isolation_state.kernel_window_denied &&
                                      isolation_state.ttbr0_switch_contract_ready &&
                                      isolation_state.supervisor_fault_boundary_preserved;
    return isolation_state.isolation_ready ? AEGIS_OK : AEGIS_EINVAL;
}

int page_isolation_selftest(void) {
    if (!isolation_state.isolation_ready) return AEGIS_EINVAL;
    if (isolation_state.address_space_count < 4U) return AEGIS_EINVAL;
    if (isolation_state.page_table_root_count != isolation_state.address_space_count) return AEGIS_EINVAL;
    if (isolation_state.guard_page_count != isolation_state.address_space_count) return AEGIS_EINVAL;
    if (isolation_state.text_rx_pages < isolation_state.address_space_count * AEGIS_PAGE_ISOLATION_TEXT_PAGES) return AEGIS_EINVAL;
    if (isolation_state.writable_nx_pages < isolation_state.address_space_count *
        (AEGIS_PAGE_ISOLATION_HEAP_PAGES + AEGIS_PAGE_ISOLATION_IPC_PAGES + AEGIS_PAGE_ISOLATION_STACK_PAGES)) return AEGIS_EINVAL;
    if (isolation_state.first_ttbr0_root == 0 || isolation_state.last_ttbr0_root == 0) return AEGIS_EINVAL;

    for (u32 i = 0; i < isolation_state.address_space_count; i++) {
        const aegis_user_address_space_t *as = &spaces[i];
        if (!as->root_allocated || as->ttbr0_root == 0) return AEGIS_EINVAL;
        if ((as->ttbr0_root & (PAGE_SIZE - 1ULL)) != 0) return AEGIS_EINVAL;
        if (!as->user_text_rx || !as->user_heap_rw_nx || !as->user_ipc_rw_nx || !as->user_stack_rw_nx) return AEGIS_EINVAL;
        if (!as->guard_unmapped || !as->kernel_window_blocked || !as->ttbr0_contract_ready) return AEGIS_EINVAL;
    }

    return AEGIS_OK;
}

const aegis_page_isolation_state_t *page_isolation_state(void) {
    return &isolation_state;
}

const aegis_user_address_space_t *page_isolation_address_space(u32 index) {
    if (index >= isolation_state.address_space_count) return 0;
    return &spaces[index];
}

bool page_isolation_ready(void) {
    return isolation_state.isolation_ready;
}

bool page_isolation_kernel_window_denied(void) {
    return isolation_state.kernel_window_denied;
}

bool page_isolation_guard_pages_unmapped(void) {
    return isolation_state.guard_pages_unmapped;
}

bool page_isolation_ttbr0_contract_ready(void) {
    return isolation_state.ttbr0_switch_contract_ready;
}
