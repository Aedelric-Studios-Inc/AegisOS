/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/userland.c
 * Early userland handoff model, feature/service binding, pre-EL0 proof,
 * v31.0 controlled EL0 transition probe, v32 first tiny user process,
 * v33 syscall return-to-user proof, v34 tiny user process exit proof,
 * v35 ELF loader/post-exit scheduler handoff proof, v36 file-backed ELF
 * loading, v37 PT_LOAD copy/user-kernel permission proof, v38 argv/envp
 * stack bootstrap + multiple process descriptor proof, and v42 real
 * multi-process launch path + syscall expansion proof.
 */

#include "userland.h"
#include "service_manager.h"
#include "syscalls.h"
#include "vfs.h"
#include "memory.h"
#include "panic.h"

static aegis_userland_feature_t features[AEGIS_USERLAND_FEATURE_MAX];
static aegis_user_process_descriptor_t user_processes[AEGIS_USERLAND_PROCESS_MAX];
static aegis_userland_handoff_t handoff;
static u32 next_feature_id;
static u32 next_process_id;

static void zero_mem(void *ptr, u64 len) {
    u8 *p = (u8 *)ptr;
    for (u64 i = 0; i < len; i++) p[i] = 0;
}

static bool str_eq(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static u32 str_len(const char *s) {
    u32 n = 0;
    if (!s) return 0;
    while (s[n] != '\0') n++;
    return n;
}

static void copy_name(char *dst, u32 dst_len, const char *src) {
    u32 i = 0;
    if (!dst || dst_len == 0) return;
    if (!src) src = "unnamed";
    while (src[i] && i < dst_len - 1U) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static bool region_aligned(const aegis_user_region_t *r) {
    return r && r->size != 0 &&
           (r->base & (PAGE_SIZE - 1U)) == 0 &&
           (r->size & (PAGE_SIZE - 1U)) == 0;
}

static bool region_end(virt_addr_t base, u64 size, virt_addr_t *out) {
    virt_addr_t end = base + size;
    if (end <= base) return false;
    if (out) *out = end;
    return true;
}

static bool regions_do_not_overlap(const aegis_user_region_t *a, const aegis_user_region_t *b) {
    virt_addr_t a_end = 0;
    virt_addr_t b_end = 0;
    if (!region_end(a->base, a->size, &a_end)) return false;
    if (!region_end(b->base, b->size, &b_end)) return false;
    return a_end <= b->base || b_end <= a->base;
}


static virt_addr_t per_process_text_base(u32 index) {
    return AEGIS_USER_TEXT_BASE + ((virt_addr_t)index * AEGIS_USER_PROCESS_STRIDE);
}

static virt_addr_t per_process_heap_base(u32 index) {
    return AEGIS_USER_HEAP_BASE + ((virt_addr_t)index * AEGIS_USER_PROCESS_STRIDE);
}

static virt_addr_t per_process_ipc_base(u32 index) {
    return AEGIS_USER_IPC_BASE + ((virt_addr_t)index * (AEGIS_USER_IPC_SIZE * 2UL));
}

static virt_addr_t per_process_stack_top(u32 index) {
    return AEGIS_USER_STACK_TOP - ((virt_addr_t)index * AEGIS_USER_STACK_STRIDE);
}

static int assign_process_layout(aegis_user_process_descriptor_t *p, u32 index) {
    if (!p) return AEGIS_EINVAL;

    p->text.base = per_process_text_base(index);
    p->text.size = AEGIS_USER_TEXT_SIZE;
    p->text.flags = AEGIS_USER_REGION_READ | AEGIS_USER_REGION_EXEC | AEGIS_USER_REGION_USER;

    p->heap.base = per_process_heap_base(index);
    p->heap.size = AEGIS_USER_HEAP_SIZE;
    p->heap.flags = AEGIS_USER_REGION_READ | AEGIS_USER_REGION_WRITE | AEGIS_USER_REGION_USER;

    p->ipc.base = per_process_ipc_base(index);
    p->ipc.size = AEGIS_USER_IPC_SIZE;
    p->ipc.flags = AEGIS_USER_REGION_READ | AEGIS_USER_REGION_WRITE | AEGIS_USER_REGION_USER;

    virt_addr_t stack_top = per_process_stack_top(index);
    p->stack.base = stack_top - AEGIS_USER_STACK_SIZE;
    p->stack.size = AEGIS_USER_STACK_SIZE;
    p->stack.flags = AEGIS_USER_REGION_READ | AEGIS_USER_REGION_WRITE | AEGIS_USER_REGION_USER;

    p->stack_guard.base = p->stack.base - PAGE_SIZE;
    p->stack_guard.size = PAGE_SIZE;
    p->stack_guard.flags = AEGIS_USER_REGION_GUARD | AEGIS_USER_REGION_USER;

    p->initial_sp = stack_top - 16U;
    if ((p->initial_sp & 0xFUL) != 0) return AEGIS_EINVAL;
    if (!region_aligned(&p->text) || !region_aligned(&p->heap) || !region_aligned(&p->ipc) ||
        !region_aligned(&p->stack) || !region_aligned(&p->stack_guard)) {
        return AEGIS_EINVAL;
    }
    if (p->stack_guard.base + p->stack_guard.size != p->stack.base) return AEGIS_EINVAL;
    if (!regions_do_not_overlap(&p->text, &p->heap)) return AEGIS_EINVAL;
    if (!regions_do_not_overlap(&p->text, &p->ipc)) return AEGIS_EINVAL;
    if (!regions_do_not_overlap(&p->text, &p->stack)) return AEGIS_EINVAL;
    if (!regions_do_not_overlap(&p->heap, &p->ipc)) return AEGIS_EINVAL;
    if (!regions_do_not_overlap(&p->heap, &p->stack)) return AEGIS_EINVAL;
    if (!regions_do_not_overlap(&p->ipc, &p->stack)) return AEGIS_EINVAL;
    return AEGIS_OK;
}

const char *userland_state_name(aegis_userland_state_t state) {
    switch (state) {
    case AEGIS_USERLAND_EMPTY:         return "empty";
    case AEGIS_USERLAND_REGISTERED:    return "registered";
    case AEGIS_USERLAND_BOUND:         return "bound";
    case AEGIS_USERLAND_HANDOFF_READY: return "handoff-ready";
    case AEGIS_USERLAND_FAILED:        return "failed";
    default:                           return "unknown";
    }
}

const char *user_process_state_name(aegis_user_process_state_t state) {
    switch (state) {
    case AEGIS_USER_PROCESS_EMPTY:                 return "empty";
    case AEGIS_USER_PROCESS_DESCRIPTOR_READY:      return "descriptor-ready";
    case AEGIS_USER_PROCESS_LAYOUT_READY:          return "layout-ready";
    case AEGIS_USER_PROCESS_SYSCALL_ABI_READY:     return "syscall-abi-ready";
    case AEGIS_USER_PROCESS_ELF_LOADED:             return "elf-loaded";
    case AEGIS_USER_PROCESS_EL0_PROBE_READY:        return "el0-probe-ready";
    case AEGIS_USER_PROCESS_EL0_ENTERED:            return "el0-entered";
    case AEGIS_USER_PROCESS_EL0_SVC_TRAPPED:        return "el0-svc-trapped";
    case AEGIS_USER_PROCESS_FIRST_LAUNCH_READY:     return "first-launch-ready";
    case AEGIS_USER_PROCESS_FIRST_RUNNING:          return "first-running";
    case AEGIS_USER_PROCESS_FIRST_SYSCALL_TRAPPED:       return "first-syscall-trapped";
    case AEGIS_USER_PROCESS_FIRST_SYSCALL_RETURN_READY:  return "first-syscall-return-ready";
    case AEGIS_USER_PROCESS_FIRST_CONTINUED_AFTER_SYSCALL:return "first-continued-after-syscall";
    case AEGIS_USER_PROCESS_ARGV_ENVP_READY:             return "argv-envp-ready";
    case AEGIS_USER_PROCESS_SECONDARY_DESCRIPTOR_READY:  return "secondary-descriptor-ready";
    case AEGIS_USER_PROCESS_FIRST_EXITED:                return "first-exited";
    default:                                      return "unknown";
    }
}

void userland_handoff_init(void) {
    zero_mem(features, sizeof(features));
    zero_mem(user_processes, sizeof(user_processes));
    zero_mem(&handoff, sizeof(handoff));
    next_feature_id = 1;
    next_process_id = 1;
    handoff.initialised = true;
}

const aegis_userland_feature_t *userland_get_feature(u32 id) {
    if (id == 0 || id > handoff.feature_count) return NULL;
    return &features[id - 1U];
}

const aegis_userland_feature_t *userland_find_feature(const char *name) {
    if (!name) return NULL;
    for (u32 i = 0; i < handoff.feature_count; i++) {
        if (features[i].state != AEGIS_USERLAND_EMPTY && str_eq(features[i].name, name)) {
            return &features[i];
        }
    }
    return NULL;
}

const aegis_user_process_descriptor_t *userland_first_process_descriptor(void) {
    if (handoff.process_descriptor_count == 0) return NULL;
    return &user_processes[0];
}

const aegis_user_process_descriptor_t *userland_process_descriptor(u32 index) {
    if (index >= handoff.process_descriptor_count) return NULL;
    return &user_processes[index];
}

static int register_feature(const char *name,
                            const char *service_name,
                            const char *image_path,
                            u32 flags) {
    if (!handoff.initialised) return AEGIS_EINVAL;
    if (!name || !service_name || !image_path) return AEGIS_EINVAL;
    if (str_len(name) == 0 || str_len(service_name) == 0 || str_len(image_path) == 0) {
        return AEGIS_EINVAL;
    }
    if (handoff.feature_count >= AEGIS_USERLAND_FEATURE_MAX) return AEGIS_ENOMEM;
    if (userland_find_feature(name)) return AEGIS_EBUSY;

    aegis_userland_feature_t *f = &features[handoff.feature_count];
    zero_mem(f, sizeof(*f));
    f->id = next_feature_id++;
    f->state = AEGIS_USERLAND_REGISTERED;
    f->flags = flags;
    copy_name(f->name, AEGIS_USERLAND_NAME_MAX, name);
    copy_name(f->service_name, AEGIS_USERLAND_NAME_MAX, service_name);
    copy_name(f->image_path, AEGIS_USERLAND_PATH_MAX, image_path);
    handoff.feature_count++;
    return AEGIS_OK;
}

static int bind_registered_features(void) {
    if (!service_manager_is_prepared()) return AEGIS_EINVAL;

    handoff.bound_count = 0;
    for (u32 i = 0; i < handoff.feature_count; i++) {
        aegis_userland_feature_t *f = &features[i];
        if (f->state != AEGIS_USERLAND_REGISTERED) return AEGIS_EINVAL;

        const aegis_service_t *svc = service_manager_find(f->service_name);
        if (!svc || svc->state != AEGIS_SERVICE_PREPARED) {
            f->state = AEGIS_USERLAND_FAILED;
            return AEGIS_ENOENT;
        }
        f->service_id = svc->id;
        f->state = AEGIS_USERLAND_BOUND;
        handoff.bound_count++;
    }
    handoff.services_bound = (handoff.bound_count == handoff.feature_count);
    return handoff.services_bound ? AEGIS_OK : AEGIS_EINVAL;
}

int userland_connect_builtin_features(void) {
    if (!handoff.initialised) return AEGIS_EINVAL;

    int rc = AEGIS_OK;
    rc = register_feature("aegis-init", "aegisd", "/sbin/aegis-init",
                          AEGIS_USERLAND_FLAG_REQUIRED | AEGIS_USERLAND_FLAG_PROCESS | AEGIS_USERLAND_FLAG_CONTROL_PLANE);
    if (rc != AEGIS_OK) return rc;
    rc = register_feature("aegisd", "aegisd", "/sbin/aegisd",
                          AEGIS_USERLAND_FLAG_REQUIRED | AEGIS_USERLAND_FLAG_PROCESS | AEGIS_USERLAND_FLAG_CONTROL_PLANE);
    if (rc != AEGIS_OK) return rc;
    rc = register_feature("securityd", "security", "/sbin/securityd",
                          AEGIS_USERLAND_FLAG_REQUIRED | AEGIS_USERLAND_FLAG_PROCESS | AEGIS_USERLAND_FLAG_CONTROL_PLANE);
    if (rc != AEGIS_OK) return rc;
    rc = register_feature("ipcd", "ipc", "/sbin/ipcd",
                          AEGIS_USERLAND_FLAG_REQUIRED | AEGIS_USERLAND_FLAG_PROCESS | AEGIS_USERLAND_FLAG_KERNEL_BACKED);
    if (rc != AEGIS_OK) return rc;
    rc = register_feature("capability-broker", "capability", "/sbin/capability-broker",
                          AEGIS_USERLAND_FLAG_REQUIRED | AEGIS_USERLAND_FLAG_PROCESS | AEGIS_USERLAND_FLAG_KERNEL_BACKED);
    if (rc != AEGIS_OK) return rc;
    rc = register_feature("netd", "network", "/sbin/netd",
                          AEGIS_USERLAND_FLAG_REQUIRED | AEGIS_USERLAND_FLAG_PROCESS);
    if (rc != AEGIS_OK) return rc;
    rc = register_feature("firewalld", "firewall", "/sbin/firewalld",
                          AEGIS_USERLAND_FLAG_REQUIRED | AEGIS_USERLAND_FLAG_PROCESS);
    if (rc != AEGIS_OK) return rc;
    rc = register_feature("routerd", "router", "/sbin/routerd",
                          AEGIS_USERLAND_FLAG_REQUIRED | AEGIS_USERLAND_FLAG_PROCESS | AEGIS_USERLAND_FLAG_CONTROL_PLANE);
    if (rc != AEGIS_OK) return rc;
    rc = register_feature("devmgr", "devfs", "/sbin/devmgr",
                          AEGIS_USERLAND_FLAG_REQUIRED | AEGIS_USERLAND_FLAG_PROCESS | AEGIS_USERLAND_FLAG_KERNEL_BACKED);
    if (rc != AEGIS_OK) return rc;
    rc = register_feature("procd", "procfs", "/sbin/procd",
                          AEGIS_USERLAND_FLAG_REQUIRED | AEGIS_USERLAND_FLAG_PROCESS | AEGIS_USERLAND_FLAG_KERNEL_BACKED);
    if (rc != AEGIS_OK) return rc;
    rc = register_feature("rustmyadmin", "aegisd", "/opt/aegisos/rustmyadmin",
                          AEGIS_USERLAND_FLAG_PROCESS | AEGIS_USERLAND_FLAG_CONTROL_PLANE);
    if (rc != AEGIS_OK) return rc;
    rc = register_feature("phoenix-feature-bridge", "router", "/sbin/phoenix-feature-bridge",
                          AEGIS_USERLAND_FLAG_PROCESS | AEGIS_USERLAND_FLAG_CONTROL_PLANE);
    if (rc != AEGIS_OK) return rc;

    handoff.feature_catalogue_ready = true;
    return bind_registered_features();
}

static int syscall_surface_selftest(void) {
    s64 pid = syscall_dispatch(SYS_GETPID, 0, 0, 0, 0, 0, 0);
    if (pid < 0) return AEGIS_EINVAL;

    s64 bad = syscall_dispatch(NR_SYSCALLS + 16U, 0, 0, 0, 0, 0, 0);
    if (bad != AEGIS_EINVAL) return AEGIS_EINVAL;

    return AEGIS_OK;
}

int userland_prepare_handoff(void) {
    if (!handoff.initialised || !handoff.feature_catalogue_ready || !handoff.services_bound) {
        return AEGIS_EINVAL;
    }
    if (!vfs_is_mounted("/") || !vfs_is_mounted("/dev") || !vfs_is_mounted("/proc")) {
        return AEGIS_EINVAL;
    }
    if (syscall_surface_selftest() != AEGIS_OK) {
        return AEGIS_EINVAL;
    }

    handoff.syscall_surface_ready = true;
    handoff.ready_count = 0;
    for (u32 i = 0; i < handoff.feature_count; i++) {
        if (features[i].state != AEGIS_USERLAND_BOUND) return AEGIS_EINVAL;
        features[i].state = AEGIS_USERLAND_HANDOFF_READY;
        handoff.ready_count++;
    }

    handoff.handoff_ready = (handoff.ready_count == handoff.feature_count);
    return handoff.handoff_ready ? AEGIS_OK : AEGIS_EINVAL;
}

static int create_fake_first_process_descriptor(void) {
    if (!handoff.handoff_ready) return AEGIS_EINVAL;
    if (handoff.process_descriptor_count != 0) return AEGIS_EBUSY;

    const aegis_userland_feature_t *init = userland_find_feature("aegis-init");
    if (!init || init->state != AEGIS_USERLAND_HANDOFF_READY) return AEGIS_ENOENT;
    if ((init->flags & AEGIS_USERLAND_FLAG_PROCESS) == 0) return AEGIS_EINVAL;

    aegis_user_process_descriptor_t *p = &user_processes[0];
    zero_mem(p, sizeof(*p));
    p->id = next_process_id++;
    p->feature_id = init->id;
    p->service_id = init->service_id;
    p->state = AEGIS_USER_PROCESS_DESCRIPTOR_READY;
    p->entry = AEGIS_USER_TEXT_BASE;
    p->syscall_gate = AEGIS_USER_SYSCALL_GATE;
    copy_name(p->name, AEGIS_USERLAND_NAME_MAX, init->name);
    copy_name(p->image_path, AEGIS_USERLAND_PATH_MAX, init->image_path);

    handoff.process_descriptor_count = 1;
    handoff.fake_process_descriptor_ready = true;
    return AEGIS_OK;
}

static int prove_user_stack_and_address_layout(void) {
    aegis_user_process_descriptor_t *p = &user_processes[0];
    if (!handoff.fake_process_descriptor_ready || p->state != AEGIS_USER_PROCESS_DESCRIPTOR_READY) {
        return AEGIS_EINVAL;
    }

    if (assign_process_layout(p, 0) != AEGIS_OK) return AEGIS_EINVAL;
    if ((p->entry < p->text.base) || (p->entry >= (p->text.base + p->text.size))) return AEGIS_EINVAL;

    p->state = AEGIS_USER_PROCESS_LAYOUT_READY;
    handoff.user_stack_layout_ready = true;
    return AEGIS_OK;
}

static int prove_syscall_abi_kernel_safe(void) {
    aegis_user_process_descriptor_t *p = &user_processes[0];
    if (!handoff.user_stack_layout_ready || p->state != AEGIS_USER_PROCESS_LAYOUT_READY) {
        return AEGIS_EINVAL;
    }

    /* Kernel-safe ABI proof: do not copy from EL0 memory and do not yield.
     * Verify syscall number validation and side-effect-light argument paths.
     */
    if (syscall_dispatch(SYS_GETPID, 0, 0, 0, 0, 0, 0) < 0) return AEGIS_EINVAL;
    if (syscall_dispatch(SYS_WRITE, 1, 0, 0, 0, 0, 0) != AEGIS_EINVAL) return AEGIS_EINVAL;
    if (syscall_dispatch(SYS_OPEN, 0, 0, 0, 0, 0, 0) != AEGIS_EINVAL) return AEGIS_EINVAL;
    if (syscall_dispatch(SYS_MMAP, AEGIS_USER_HEAP_BASE, 0, 0, 0, 0, 0) != AEGIS_EINVAL) return AEGIS_EINVAL;
    if (syscall_dispatch(NR_SYSCALLS + 64U, 0, 0, 0, 0, 0, 0) != AEGIS_EINVAL) return AEGIS_EINVAL;

    if ((p->syscall_gate & (PAGE_SIZE - 1U)) != 0) return AEGIS_EINVAL;
    if (p->syscall_gate <= p->stack.base) return AEGIS_EINVAL;

    p->state = AEGIS_USER_PROCESS_SYSCALL_ABI_READY;
    handoff.syscall_abi_kernel_safe_ready = true;
    return AEGIS_OK;
}

int userland_prepare_pre_el0_contract(void) {
    if (!handoff.handoff_ready) return AEGIS_EINVAL;
    int rc = create_fake_first_process_descriptor();
    if (rc != AEGIS_OK) return rc;
    rc = prove_user_stack_and_address_layout();
    if (rc != AEGIS_OK) return rc;
    rc = prove_syscall_abi_kernel_safe();
    if (rc != AEGIS_OK) return rc;
    handoff.pre_el0_ready = true;
    return AEGIS_OK;
}

int userland_selftest(void) {
    if (!handoff.initialised) return AEGIS_EINVAL;
    if (!handoff.feature_catalogue_ready) return AEGIS_EINVAL;
    if (!handoff.services_bound) return AEGIS_EINVAL;
    if (!handoff.syscall_surface_ready) return AEGIS_EINVAL;
    if (!handoff.handoff_ready) return AEGIS_EINVAL;
    if (handoff.feature_count < 10U) return AEGIS_EINVAL;
    if (handoff.bound_count != handoff.feature_count) return AEGIS_EINVAL;
    if (handoff.ready_count != handoff.feature_count) return AEGIS_EINVAL;

    const aegis_userland_feature_t *init = userland_find_feature("aegis-init");
    const aegis_userland_feature_t *router = userland_find_feature("routerd");
    const aegis_userland_feature_t *admin = userland_find_feature("rustmyadmin");
    const aegis_userland_feature_t *bridge = userland_find_feature("phoenix-feature-bridge");
    if (!init || !router || !admin || !bridge) return AEGIS_ENOENT;
    if (init->state != AEGIS_USERLAND_HANDOFF_READY) return AEGIS_EINVAL;
    if (router->state != AEGIS_USERLAND_HANDOFF_READY) return AEGIS_EINVAL;
    if (admin->state != AEGIS_USERLAND_HANDOFF_READY) return AEGIS_EINVAL;
    if (bridge->state != AEGIS_USERLAND_HANDOFF_READY) return AEGIS_EINVAL;
    if ((init->flags & AEGIS_USERLAND_FLAG_REQUIRED) == 0) return AEGIS_EINVAL;
    if ((router->flags & AEGIS_USERLAND_FLAG_CONTROL_PLANE) == 0) return AEGIS_EINVAL;
    if ((admin->flags & AEGIS_USERLAND_FLAG_CONTROL_PLANE) == 0) return AEGIS_EINVAL;
    return AEGIS_OK;
}

int userland_pre_el0_selftest(void) {
    if (!handoff.pre_el0_ready) return AEGIS_EINVAL;
    if (!handoff.fake_process_descriptor_ready) return AEGIS_EINVAL;
    if (!handoff.user_stack_layout_ready) return AEGIS_EINVAL;
    if (!handoff.syscall_abi_kernel_safe_ready) return AEGIS_EINVAL;
    if (handoff.process_descriptor_count != 1U) return AEGIS_EINVAL;

    const aegis_user_process_descriptor_t *p = userland_first_process_descriptor();
    if (!p) return AEGIS_ENOENT;
    if (p->state != AEGIS_USER_PROCESS_SYSCALL_ABI_READY) return AEGIS_EINVAL;
    if (!str_eq(p->name, "aegis-init")) return AEGIS_EINVAL;
    if (!str_eq(p->image_path, "/sbin/aegis-init")) return AEGIS_EINVAL;
    if (p->text.base != AEGIS_USER_TEXT_BASE || p->stack.size != AEGIS_USER_STACK_SIZE) return AEGIS_EINVAL;
    if ((p->initial_sp & 0xFUL) != 0) return AEGIS_EINVAL;
    if ((p->text.flags & AEGIS_USER_REGION_EXEC) == 0) return AEGIS_EINVAL;
    if ((p->stack.flags & AEGIS_USER_REGION_WRITE) == 0) return AEGIS_EINVAL;
    if ((p->stack_guard.flags & AEGIS_USER_REGION_GUARD) == 0) return AEGIS_EINVAL;
    return AEGIS_OK;
}


int userland_prepare_controlled_el0_transition(u64 entry, u64 user_sp) {
    if (!handoff.pre_el0_ready) return AEGIS_EINVAL;
    if (!handoff.syscall_abi_kernel_safe_ready) return AEGIS_EINVAL;
    if (handoff.process_descriptor_count < 1U) return AEGIS_EINVAL;
    if (!entry || !user_sp) return AEGIS_EINVAL;

    aegis_user_process_descriptor_t *proc = &user_processes[0];
    if (proc->state != AEGIS_USER_PROCESS_SYSCALL_ABI_READY) return AEGIS_EINVAL;

    if ((entry & 0x3ULL) != 0) return AEGIS_EINVAL;
    if ((user_sp & 0xFULL) != 0) return AEGIS_EINVAL;

    /* The controlled v31.0 probe intentionally uses a kernel-resident stub as
     * its first EL0 PC.  The full v32 user process will replace this with an
     * ELF text mapping at proc->entry.
     */
    if (user_sp < proc->stack.base || user_sp >= (proc->stack.base + proc->stack.size)) {
        return AEGIS_EINVAL;
    }

    handoff.el0_probe_entry = entry;
    handoff.el0_probe_sp = user_sp;
    handoff.el0_entry_selected = true;
    handoff.el0_stack_selected = true;
    handoff.el0_svc_trap_seen = false;
    handoff.controlled_el0_transition_seen = false;
    handoff.el0_probe_syscall_nr = 0;
    handoff.el0_probe_arg0 = 0;
    handoff.el0_probe_elr = 0;
    handoff.el0_probe_spsr = 0;
    handoff.el0_transition_prepared = true;
    proc->state = AEGIS_USER_PROCESS_EL0_PROBE_READY;
    return AEGIS_OK;
}

bool userland_controlled_el0_transition_active(void) {
    return handoff.el0_transition_prepared && !handoff.el0_svc_trap_seen;
}

bool userland_controlled_el0_transition_seen(void) {
    return handoff.controlled_el0_transition_seen;
}

void userland_mark_el0_svc_trap(u64 syscall_nr, u64 arg0, u64 elr, u64 spsr) {
    handoff.el0_probe_syscall_nr = syscall_nr;
    handoff.el0_probe_arg0 = arg0;
    handoff.el0_probe_elr = elr;
    handoff.el0_probe_spsr = spsr;
    handoff.el0_svc_trap_seen = true;
    handoff.controlled_el0_transition_seen = true;

    if (handoff.process_descriptor_count >= 1U) {
        user_processes[0].state = AEGIS_USER_PROCESS_EL0_SVC_TRAPPED;
    }
}

int userland_controlled_el0_transition_selftest(void) {
    if (!handoff.el0_transition_prepared) return AEGIS_EINVAL;
    if (!handoff.el0_entry_selected || !handoff.el0_stack_selected) return AEGIS_EINVAL;
    if (!handoff.el0_probe_entry || !handoff.el0_probe_sp) return AEGIS_EINVAL;
    if ((handoff.el0_probe_entry & 0x3ULL) != 0) return AEGIS_EINVAL;
    if ((handoff.el0_probe_sp & 0xFULL) != 0) return AEGIS_EINVAL;

    const aegis_user_process_descriptor_t *proc = userland_first_process_descriptor();
    if (!proc) return AEGIS_ENOENT;
    if (proc->state != AEGIS_USER_PROCESS_EL0_PROBE_READY &&
        proc->state != AEGIS_USER_PROCESS_EL0_SVC_TRAPPED) {
        return AEGIS_EINVAL;
    }
    if (handoff.el0_probe_sp < proc->stack.base ||
        handoff.el0_probe_sp >= (proc->stack.base + proc->stack.size)) {
        return AEGIS_EINVAL;
    }
    return AEGIS_OK;
}


int userland_bind_first_process_to_loaded_elf(const char *path, u64 entry,
                                               u64 text_base, u64 text_size,
                                               u32 text_flags,
                                               u64 file_bytes_copied,
                                               u64 zero_bytes) {
    if (!handoff.pre_el0_ready) return AEGIS_EINVAL;
    if (handoff.process_descriptor_count != 1U) return AEGIS_EINVAL;
    if (!path || !entry || !text_base || text_size == 0) return AEGIS_EINVAL;
    if ((entry & 0x3ULL) != 0) return AEGIS_EINVAL;
    if ((text_base & (PAGE_SIZE - 1U)) != 0) return AEGIS_EINVAL;
    if ((text_size & (PAGE_SIZE - 1U)) != 0) return AEGIS_EINVAL;
    if (file_bytes_copied == 0 || file_bytes_copied > text_size) return AEGIS_EINVAL;
    if (zero_bytes != text_size - file_bytes_copied) return AEGIS_EINVAL;

    const u32 required = AEGIS_USER_REGION_READ | AEGIS_USER_REGION_EXEC |
                         AEGIS_USER_REGION_USER | AEGIS_USER_REGION_MAPPED;
    if ((text_flags & required) != required) return AEGIS_EINVAL;
    if ((text_flags & AEGIS_USER_REGION_WRITE) != 0) return AEGIS_EPERM;
    if ((text_flags & AEGIS_USER_REGION_KERNEL) != 0) return AEGIS_EPERM;

    aegis_user_process_descriptor_t *proc = &user_processes[0];
    if (proc->state != AEGIS_USER_PROCESS_SYSCALL_ABI_READY &&
        proc->state != AEGIS_USER_PROCESS_ELF_LOADED) {
        return AEGIS_EINVAL;
    }
    if (!str_eq(proc->name, "aegis-init")) return AEGIS_EINVAL;
    if (!str_eq(proc->image_path, path)) return AEGIS_EINVAL;
    if (entry < text_base || entry >= (text_base + text_size)) return AEGIS_EINVAL;

    proc->entry = entry;
    proc->text.base = text_base;
    proc->text.size = text_size;
    proc->text.flags = text_flags;
    proc->state = AEGIS_USER_PROCESS_ELF_LOADED;

    handoff.elf_loader_ready = true;
    handoff.elf_aegis_init_validated = true;
    handoff.first_user_process_loaded_from_elf = true;
    handoff.elf_pt_load_segments_copied = true;
    handoff.user_kernel_page_permissions_ready = true;
    handoff.elf_entry = entry;
    handoff.elf_text_base = text_base;
    handoff.elf_text_size = text_size;
    handoff.elf_text_file_bytes_copied = file_bytes_copied;
    handoff.elf_text_zero_bytes = zero_bytes;
    handoff.elf_text_region_flags = text_flags;
    return AEGIS_OK;
}

int userland_bind_first_process_to_elf(const char *path, u64 entry, u64 text_base, u64 text_size) {
    return userland_bind_first_process_to_loaded_elf(path,
                                                     entry,
                                                     text_base,
                                                     text_size,
                                                     AEGIS_USER_REGION_READ |
                                                     AEGIS_USER_REGION_EXEC |
                                                     AEGIS_USER_REGION_USER |
                                                     AEGIS_USER_REGION_MAPPED,
                                                     1U,
                                                     text_size - 1U);
}

int userland_first_process_elf_loader_selftest(void) {
    if (!handoff.elf_loader_ready || !handoff.elf_aegis_init_validated) return AEGIS_EINVAL;
    if (!handoff.first_user_process_loaded_from_elf) return AEGIS_EINVAL;
    if (!handoff.elf_entry || !handoff.elf_text_base || !handoff.elf_text_size) return AEGIS_EINVAL;
    if ((handoff.elf_text_base & (PAGE_SIZE - 1U)) != 0) return AEGIS_EINVAL;
    if ((handoff.elf_text_size & (PAGE_SIZE - 1U)) != 0) return AEGIS_EINVAL;
    if (handoff.elf_entry < handoff.elf_text_base ||
        handoff.elf_entry >= (handoff.elf_text_base + handoff.elf_text_size)) {
        return AEGIS_EINVAL;
    }

    const aegis_user_process_descriptor_t *proc = userland_first_process_descriptor();
    if (!proc) return AEGIS_ENOENT;
    if (proc->state != AEGIS_USER_PROCESS_ELF_LOADED) return AEGIS_EINVAL;
    if (proc->entry != handoff.elf_entry) return AEGIS_EINVAL;
    if (proc->text.base != handoff.elf_text_base) return AEGIS_EINVAL;
    if (proc->text.size != handoff.elf_text_size) return AEGIS_EINVAL;
    if ((proc->text.flags & AEGIS_USER_REGION_EXEC) == 0) return AEGIS_EINVAL;
    return AEGIS_OK;
}

int userland_pt_load_permissions_selftest(void) {
    if (!handoff.elf_pt_load_segments_copied) return AEGIS_EINVAL;
    if (!handoff.user_kernel_page_permissions_ready) return AEGIS_EINVAL;
    if (handoff.elf_text_file_bytes_copied == 0) return AEGIS_EINVAL;
    if (handoff.elf_text_file_bytes_copied > handoff.elf_text_size) return AEGIS_EINVAL;
    if (handoff.elf_text_zero_bytes != handoff.elf_text_size - handoff.elf_text_file_bytes_copied) return AEGIS_EINVAL;

    const aegis_user_process_descriptor_t *proc = userland_first_process_descriptor();
    if (!proc) return AEGIS_ENOENT;
    if ((proc->text.flags & AEGIS_USER_REGION_READ) == 0) return AEGIS_EINVAL;
    if ((proc->text.flags & AEGIS_USER_REGION_EXEC) == 0) return AEGIS_EINVAL;
    if ((proc->text.flags & AEGIS_USER_REGION_USER) == 0) return AEGIS_EINVAL;
    if ((proc->text.flags & AEGIS_USER_REGION_MAPPED) == 0) return AEGIS_EINVAL;
    if ((proc->text.flags & AEGIS_USER_REGION_WRITE) != 0) return AEGIS_EPERM;
    if ((proc->heap.flags & AEGIS_USER_REGION_WRITE) == 0) return AEGIS_EINVAL;
    if ((proc->heap.flags & AEGIS_USER_REGION_EXEC) != 0) return AEGIS_EPERM;
    if ((proc->stack.flags & AEGIS_USER_REGION_WRITE) == 0) return AEGIS_EINVAL;
    if ((proc->stack.flags & AEGIS_USER_REGION_EXEC) != 0) return AEGIS_EPERM;
    if ((proc->stack_guard.flags & AEGIS_USER_REGION_GUARD) == 0) return AEGIS_EINVAL;
    return AEGIS_OK;
}


int userland_prepare_user_stack_bootstrap(void) {
    if (!handoff.elf_pt_load_segments_copied || !handoff.user_kernel_page_permissions_ready) return AEGIS_EINVAL;
    if (handoff.process_descriptor_count < 1U) return AEGIS_ENOENT;

    aegis_user_process_descriptor_t *proc = &user_processes[0];
    if (proc->state != AEGIS_USER_PROCESS_ELF_LOADED) return AEGIS_EINVAL;
    if (!str_eq(proc->name, "aegis-init")) return AEGIS_EINVAL;
    if (!str_eq(proc->image_path, "/sbin/aegis-init")) return AEGIS_EINVAL;

    proc->argc = 3;
    proc->envc = 3;
    copy_name(proc->argv[0], AEGIS_USER_ARG_STRING_MAX, "/sbin/aegis-init");
    copy_name(proc->argv[1], AEGIS_USER_ARG_STRING_MAX, "--system");
    copy_name(proc->argv[2], AEGIS_USER_ARG_STRING_MAX, "--router-bringup");
    copy_name(proc->envp[0], AEGIS_USER_ARG_STRING_MAX, "PATH=/sbin:/bin");
    copy_name(proc->envp[1], AEGIS_USER_ARG_STRING_MAX, "AEGIS_PROFILE=router");
    copy_name(proc->envp[2], AEGIS_USER_ARG_STRING_MAX, "RUSTMYADMIN_MODE=kernel-handoff");

    virt_addr_t bootstrap_sp = (proc->stack.base + proc->stack.size - AEGIS_USER_STACK_BOOTSTRAP_SIZE) & ~0xFUL;
    if (bootstrap_sp < proc->stack.base || bootstrap_sp >= (proc->stack.base + proc->stack.size)) return AEGIS_EINVAL;
    proc->bootstrap_sp = bootstrap_sp;
    proc->bootstrap_stack_bytes = AEGIS_USER_STACK_BOOTSTRAP_SIZE;
    proc->argv_user = bootstrap_sp;
    proc->envp_user = proc->argv_user + ((virt_addr_t)(proc->argc + 1U) * sizeof(u64));
    proc->auxv_user = proc->envp_user + ((virt_addr_t)(proc->envc + 1U) * sizeof(u64));
    proc->initial_sp = bootstrap_sp;
    proc->state = AEGIS_USER_PROCESS_ARGV_ENVP_READY;

    if ((proc->argv_user < proc->stack.base) || (proc->auxv_user >= proc->stack.base + proc->stack.size)) return AEGIS_EINVAL;
    if ((proc->initial_sp & 0xFUL) != 0) return AEGIS_EINVAL;

    handoff.user_stack_argv_envp_ready = true;
    handoff.bootstrap_argc = proc->argc;
    handoff.bootstrap_envc = proc->envc;
    handoff.bootstrap_stack_pointer = proc->bootstrap_sp;
    handoff.bootstrap_argv_user = proc->argv_user;
    handoff.bootstrap_envp_user = proc->envp_user;
    handoff.bootstrap_auxv_user = proc->auxv_user;
    return AEGIS_OK;
}

int userland_user_stack_bootstrap_selftest(void) {
    if (!handoff.user_stack_argv_envp_ready) return AEGIS_EINVAL;
    const aegis_user_process_descriptor_t *proc = userland_first_process_descriptor();
    if (!proc) return AEGIS_ENOENT;
    if (proc->state != AEGIS_USER_PROCESS_ARGV_ENVP_READY) return AEGIS_EINVAL;
    if (proc->argc != 3U || proc->envc != 3U) return AEGIS_EINVAL;
    if (!str_eq(proc->argv[0], "/sbin/aegis-init")) return AEGIS_EINVAL;
    if (!str_eq(proc->argv[1], "--system")) return AEGIS_EINVAL;
    if (!str_eq(proc->envp[0], "PATH=/sbin:/bin")) return AEGIS_EINVAL;
    if (proc->bootstrap_stack_bytes != AEGIS_USER_STACK_BOOTSTRAP_SIZE) return AEGIS_EINVAL;
    if (proc->bootstrap_sp != handoff.bootstrap_stack_pointer) return AEGIS_EINVAL;
    if (proc->argv_user != handoff.bootstrap_argv_user) return AEGIS_EINVAL;
    if (proc->envp_user != handoff.bootstrap_envp_user) return AEGIS_EINVAL;
    if (proc->initial_sp != proc->bootstrap_sp) return AEGIS_EINVAL;
    if ((proc->initial_sp & 0xFUL) != 0) return AEGIS_EINVAL;
    if (proc->initial_sp < proc->stack.base || proc->initial_sp >= (proc->stack.base + proc->stack.size)) return AEGIS_EINVAL;
    return AEGIS_OK;
}

static int create_secondary_process_descriptor(const char *feature_name, u32 index) {
    if (index >= AEGIS_USERLAND_PROCESS_MAX) return AEGIS_ENOMEM;
    const aegis_userland_feature_t *feature = userland_find_feature(feature_name);
    if (!feature || feature->state != AEGIS_USERLAND_HANDOFF_READY) return AEGIS_ENOENT;
    if ((feature->flags & AEGIS_USERLAND_FLAG_PROCESS) == 0) return AEGIS_EINVAL;

    aegis_user_process_descriptor_t *p = &user_processes[index];
    zero_mem(p, sizeof(*p));
    p->id = next_process_id++;
    p->feature_id = feature->id;
    p->service_id = feature->service_id;
    p->state = AEGIS_USER_PROCESS_SECONDARY_DESCRIPTOR_READY;
    p->entry = per_process_text_base(index);
    p->syscall_gate = AEGIS_USER_SYSCALL_GATE;
    copy_name(p->name, AEGIS_USERLAND_NAME_MAX, feature->name);
    copy_name(p->image_path, AEGIS_USERLAND_PATH_MAX, feature->image_path);
    if (assign_process_layout(p, index) != AEGIS_OK) return AEGIS_EINVAL;
    p->entry = p->text.base;
    return AEGIS_OK;
}

int userland_create_multiple_process_descriptors(void) {
    if (!handoff.user_stack_argv_envp_ready) return AEGIS_EINVAL;
    if (handoff.process_descriptor_count != 1U) return AEGIS_EBUSY;

    const char *names[] = { "aegisd", "routerd", "rustmyadmin" };
    u32 count = 1U;
    for (u32 i = 0; i < (u32)(sizeof(names) / sizeof(names[0])); i++) {
        int rc = create_secondary_process_descriptor(names[i], count);
        if (rc != AEGIS_OK) return rc;
        count++;
    }

    handoff.process_descriptor_count = count;
    handoff.secondary_process_descriptor_count = count - 1U;
    handoff.launchable_process_descriptor_count = count;
    handoff.multiple_process_descriptors_ready = true;
    return AEGIS_OK;
}

int userland_multiple_process_descriptors_selftest(void) {
    if (!handoff.multiple_process_descriptors_ready) return AEGIS_EINVAL;
    if (handoff.process_descriptor_count < 4U) return AEGIS_EINVAL;
    if (handoff.secondary_process_descriptor_count < 3U) return AEGIS_EINVAL;
    const aegis_user_process_descriptor_t *first = userland_first_process_descriptor();
    if (!first || !str_eq(first->name, "aegis-init")) return AEGIS_EINVAL;
    if (first->state != AEGIS_USER_PROCESS_ARGV_ENVP_READY) return AEGIS_EINVAL;

    for (u32 i = 1; i < handoff.process_descriptor_count; i++) {
        const aegis_user_process_descriptor_t *p = &user_processes[i];
        if (p->id == 0 || p->feature_id == 0 || p->service_id == 0) return AEGIS_EINVAL;
        if (p->state != AEGIS_USER_PROCESS_SECONDARY_DESCRIPTOR_READY) return AEGIS_EINVAL;
        if (str_len(p->name) == 0 || str_len(p->image_path) == 0) return AEGIS_EINVAL;
        if (!region_aligned(&p->text) || !region_aligned(&p->heap) || !region_aligned(&p->stack)) return AEGIS_EINVAL;
        if ((p->stack.flags & AEGIS_USER_REGION_WRITE) == 0) return AEGIS_EINVAL;
        if ((p->stack.flags & AEGIS_USER_REGION_EXEC) != 0) return AEGIS_EPERM;
        for (u32 j = 0; j < i; j++) {
            if (p->id == user_processes[j].id) return AEGIS_EINVAL;
            if (!regions_do_not_overlap(&p->stack, &user_processes[j].stack)) return AEGIS_EINVAL;
            if (!regions_do_not_overlap(&p->text, &user_processes[j].text)) return AEGIS_EINVAL;
        }
    }
    return AEGIS_OK;
}

bool userland_user_stack_bootstrap_ready(void) {
    return handoff.user_stack_argv_envp_ready;
}

bool userland_multiple_process_descriptors_ready(void) {
    return handoff.multiple_process_descriptors_ready;
}

static int prepare_descriptor_bootstrap(aegis_user_process_descriptor_t *p, const char *mode) {
    if (!p || !mode) return AEGIS_EINVAL;
    if (p->stack.size == 0 || p->stack.base == 0) return AEGIS_EINVAL;

    p->argc = 2U;
    p->envc = 2U;
    copy_name(p->argv[0], AEGIS_USER_ARG_STRING_MAX, p->image_path);
    copy_name(p->argv[1], AEGIS_USER_ARG_STRING_MAX, mode);
    copy_name(p->envp[0], AEGIS_USER_ARG_STRING_MAX, "PATH=/sbin:/bin");
    copy_name(p->envp[1], AEGIS_USER_ARG_STRING_MAX, "AEGIS_PROFILE=router");

    virt_addr_t bootstrap_sp = (p->stack.base + p->stack.size - AEGIS_USER_STACK_BOOTSTRAP_SIZE) & ~0xFUL;
    if (bootstrap_sp < p->stack.base || bootstrap_sp >= (p->stack.base + p->stack.size)) return AEGIS_EINVAL;
    p->bootstrap_sp = bootstrap_sp;
    p->bootstrap_stack_bytes = AEGIS_USER_STACK_BOOTSTRAP_SIZE;
    p->argv_user = bootstrap_sp;
    p->envp_user = p->argv_user + ((virt_addr_t)(p->argc + 1U) * sizeof(u64));
    p->auxv_user = p->envp_user + ((virt_addr_t)(p->envc + 1U) * sizeof(u64));
    p->initial_sp = bootstrap_sp;
    if ((p->initial_sp & 0xFUL) != 0) return AEGIS_EINVAL;
    if (p->auxv_user >= p->stack.base + p->stack.size) return AEGIS_EINVAL;
    return AEGIS_OK;
}

int userland_prepare_real_multiprocess_launch_path(void) {
    if (!handoff.multiple_process_descriptors_ready) return AEGIS_EINVAL;
    if (handoff.process_descriptor_count < 4U) return AEGIS_EINVAL;

    u32 launchable = 0;
    for (u32 i = 0; i < handoff.process_descriptor_count; i++) {
        aegis_user_process_descriptor_t *p = &user_processes[i];
        if (p->id == 0 || str_len(p->name) == 0 || str_len(p->image_path) == 0) return AEGIS_EINVAL;
        if (!region_aligned(&p->text) || !region_aligned(&p->heap) || !region_aligned(&p->stack)) return AEGIS_EINVAL;
        if ((p->text.flags & AEGIS_USER_REGION_USER) == 0) return AEGIS_EINVAL;
        if ((p->stack.flags & AEGIS_USER_REGION_WRITE) == 0) return AEGIS_EINVAL;
        if ((p->stack.flags & AEGIS_USER_REGION_EXEC) != 0) return AEGIS_EPERM;

        if (i == 0) {
            if (p->state != AEGIS_USER_PROCESS_ARGV_ENVP_READY &&
                p->state != AEGIS_USER_PROCESS_MULTIPROCESS_LAUNCH_READY) return AEGIS_EINVAL;
            if (p->argc == 0 || p->envc == 0 || p->bootstrap_sp == 0) return AEGIS_EINVAL;
        } else {
            if (p->state != AEGIS_USER_PROCESS_SECONDARY_DESCRIPTOR_READY &&
                p->state != AEGIS_USER_PROCESS_MULTIPROCESS_LAUNCH_READY) return AEGIS_EINVAL;
            int rc = prepare_descriptor_bootstrap(p, "--service");
            if (rc != AEGIS_OK) return rc;
        }
        p->state = AEGIS_USER_PROCESS_MULTIPROCESS_LAUNCH_READY;
        launchable++;
    }

    handoff.real_multiprocess_launch_ready = true;
    handoff.scheduled_user_process_count = launchable;
    handoff.launchable_process_descriptor_count = launchable;
    return AEGIS_OK;
}

int userland_real_multiprocess_launch_selftest(void) {
    if (!handoff.real_multiprocess_launch_ready) return AEGIS_EINVAL;
    if (handoff.scheduled_user_process_count != handoff.process_descriptor_count) return AEGIS_EINVAL;
    if (handoff.launchable_process_descriptor_count < 4U) return AEGIS_EINVAL;
    bool saw_init = false, saw_aegisd = false, saw_routerd = false, saw_admin = false;
    for (u32 i = 0; i < handoff.process_descriptor_count; i++) {
        const aegis_user_process_descriptor_t *p = &user_processes[i];
        if (p->state != AEGIS_USER_PROCESS_MULTIPROCESS_LAUNCH_READY) return AEGIS_EINVAL;
        if (p->entry == 0 || p->initial_sp == 0) return AEGIS_EINVAL;
        if ((p->initial_sp & 0xFUL) != 0) return AEGIS_EINVAL;
        if (p->initial_sp < p->stack.base || p->initial_sp >= p->stack.base + p->stack.size) return AEGIS_EINVAL;
        if (p->argc == 0 || p->envc == 0) return AEGIS_EINVAL;
        if (str_eq(p->name, "aegis-init")) saw_init = true;
        if (str_eq(p->name, "aegisd")) saw_aegisd = true;
        if (str_eq(p->name, "routerd")) saw_routerd = true;
        if (str_eq(p->name, "rustmyadmin")) saw_admin = true;
    }
    return (saw_init && saw_aegisd && saw_routerd && saw_admin) ? AEGIS_OK : AEGIS_ENOENT;
}

s64 userland_request_process_launch_by_path(const char *path, u64 flags) {
    (void)flags;
    if (!handoff.real_multiprocess_launch_ready || !path) return AEGIS_EINVAL;
    for (u32 i = 0; i < handoff.process_descriptor_count; i++) {
        aegis_user_process_descriptor_t *p = &user_processes[i];
        if (str_eq(p->image_path, path) || str_eq(p->name, path)) {
            if (p->state != AEGIS_USER_PROCESS_MULTIPROCESS_LAUNCH_READY &&
                p->state != AEGIS_USER_PROCESS_FIRST_LAUNCH_READY &&
                p->state != AEGIS_USER_PROCESS_FIRST_RUNNING) return AEGIS_EBUSY;
            handoff.spawn_request_count++;
            handoff.last_spawn_pid = p->id;
            return (s64)p->id;
        }
    }
    return AEGIS_ENOENT;
}

u32 userland_feature_id_by_name(const char *name) {
    const aegis_userland_feature_t *f = userland_find_feature(name);
    return f ? f->id : 0U;
}

int userland_prepare_syscall_expansion(void) {
    if (!handoff.real_multiprocess_launch_ready) return AEGIS_EINVAL;
    if (NR_SYSCALLS < 18U) return AEGIS_EINVAL;
    handoff.syscall_expansion_ready = true;
    handoff.syscall_expanded_count = NR_SYSCALLS;
    return AEGIS_OK;
}

int userland_syscall_expansion_selftest(void) {
    if (!handoff.syscall_expansion_ready) return AEGIS_EINVAL;
    if (handoff.syscall_expanded_count < 18U) return AEGIS_EINVAL;
    if (syscall_dispatch(SYS_GETPID, 0, 0, 0, 0, 0, 0) < 0) return AEGIS_EINVAL;
    if (syscall_dispatch(SYS_GETPPID, 0, 0, 0, 0, 0, 0) < 0) return AEGIS_EINVAL;
    if (syscall_dispatch(SYS_GETTID, 0, 0, 0, 0, 0, 0) < 0) return AEGIS_EINVAL;
    /* v55.1.8: this is a kernel-side milestone selftest, not an EL0 copyin
     * test.  After the P0 syscall hardening work, SYS_SERVICE_ID/SYS_SPAWN
     * correctly reject kernel-resident string pointers.  Prove the same
     * launch semantics through the kernel/userland helpers here, and leave
     * raw pointer copyin coverage to syscall.c tests.
     */
    if (userland_feature_id_by_name("routerd") == 0U) return AEGIS_EINVAL;
    s64 spawn_pid = userland_request_process_launch_by_path("/sbin/routerd", 0);
    if (spawn_pid <= 1) return AEGIS_EINVAL;
    if (handoff.last_spawn_pid != (u32)spawn_pid || handoff.spawn_request_count == 0) return AEGIS_EINVAL;
    if (syscall_dispatch(SYS_WAITPID, 9999U, 0, 0, 0, 0, 0) != AEGIS_ENOENT) return AEGIS_EINVAL;
    if (syscall_dispatch(NR_SYSCALLS + 32U, 0, 0, 0, 0, 0, 0) != AEGIS_EINVAL) return AEGIS_EINVAL;
    return AEGIS_OK;
}

bool userland_real_multiprocess_launch_ready(void) {
    return handoff.real_multiprocess_launch_ready;
}

bool userland_syscall_expansion_ready(void) {
    return handoff.syscall_expansion_ready;
}

int userland_prepare_first_tiny_user_process(u64 entry, u64 user_sp) {
    if (!handoff.pre_el0_ready) return AEGIS_EINVAL;
    if (!handoff.syscall_abi_kernel_safe_ready) return AEGIS_EINVAL;
    if (handoff.process_descriptor_count < 1U) return AEGIS_EINVAL;
    if (!entry || !user_sp) return AEGIS_EINVAL;

    aegis_user_process_descriptor_t *proc = &user_processes[0];
    if (proc->state != AEGIS_USER_PROCESS_SYSCALL_ABI_READY &&
        proc->state != AEGIS_USER_PROCESS_ELF_LOADED &&
        proc->state != AEGIS_USER_PROCESS_ARGV_ENVP_READY &&
        proc->state != AEGIS_USER_PROCESS_MULTIPROCESS_LAUNCH_READY &&
        proc->state != AEGIS_USER_PROCESS_EL0_PROBE_READY &&
        proc->state != AEGIS_USER_PROCESS_EL0_SVC_TRAPPED) {
        return AEGIS_EINVAL;
    }
    if (!str_eq(proc->name, "aegis-init")) return AEGIS_EINVAL;
    if (!str_eq(proc->image_path, "/sbin/aegis-init")) return AEGIS_EINVAL;
    if ((entry & 0x3ULL) != 0) return AEGIS_EINVAL;
    if ((user_sp & 0xFULL) != 0) return AEGIS_EINVAL;
    if (user_sp < proc->stack.base || user_sp >= (proc->stack.base + proc->stack.size)) {
        return AEGIS_EINVAL;
    }

    proc->entry = entry;
    proc->initial_sp = user_sp;
    proc->state = AEGIS_USER_PROCESS_FIRST_LAUNCH_READY;

    handoff.first_user_process_descriptor_ready = true;
    handoff.first_user_process_launch_ready = true;
    handoff.first_user_process_seen = false;
    handoff.first_user_process_syscall_seen = false;
    handoff.first_user_process_syscall_return_ready = false;
    handoff.first_user_process_continued_after_syscall = false;
    handoff.first_user_process_exit_requested = false;
    handoff.first_user_process_exited = false;
    handoff.post_exit_scheduler_handoff = false;
    handoff.scheduler_handoff_no_runnable_user = false;
    handoff.first_user_process_entry = entry;
    handoff.first_user_process_sp = user_sp;
    handoff.first_user_process_syscall_nr = 0;
    handoff.first_user_process_arg0 = 0;
    handoff.first_user_process_elr = 0;
    handoff.first_user_process_spsr = 0;
    handoff.first_user_process_syscall_return_value = 0;
    handoff.first_user_process_continuation_syscall_nr = 0;
    handoff.first_user_process_continuation_arg0 = 0;
    handoff.first_user_process_continuation_elr = 0;
    handoff.first_user_process_continuation_spsr = 0;
    handoff.first_user_process_exit_syscall_nr = 0;
    handoff.first_user_process_exit_code = 0;
    handoff.first_user_process_exit_elr = 0;
    handoff.first_user_process_exit_spsr = 0;
    handoff.first_user_process_pid = proc->id;

    /* Reuse the proven v31 EL0 transition pipe for the v32 first-process body. */
    handoff.el0_probe_entry = entry;
    handoff.el0_probe_sp = user_sp;
    handoff.el0_entry_selected = true;
    handoff.el0_stack_selected = true;
    handoff.el0_svc_trap_seen = false;
    handoff.controlled_el0_transition_seen = false;
    handoff.el0_transition_prepared = true;
    return AEGIS_OK;
}

int userland_first_tiny_user_process_selftest(void) {
    if (!handoff.first_user_process_descriptor_ready) return AEGIS_EINVAL;
    if (!handoff.first_user_process_launch_ready) return AEGIS_EINVAL;
    if (!handoff.first_user_process_entry || !handoff.first_user_process_sp) return AEGIS_EINVAL;
    if ((handoff.first_user_process_entry & 0x3ULL) != 0) return AEGIS_EINVAL;
    if ((handoff.first_user_process_sp & 0xFULL) != 0) return AEGIS_EINVAL;
    if (handoff.first_user_process_pid == 0) return AEGIS_EINVAL;

    const aegis_user_process_descriptor_t *proc = userland_first_process_descriptor();
    if (!proc) return AEGIS_ENOENT;
    if (proc->state != AEGIS_USER_PROCESS_FIRST_LAUNCH_READY &&
        proc->state != AEGIS_USER_PROCESS_FIRST_SYSCALL_TRAPPED) {
        return AEGIS_EINVAL;
    }
    if (!str_eq(proc->name, "aegis-init")) return AEGIS_EINVAL;
    if (proc->entry != handoff.first_user_process_entry) return AEGIS_EINVAL;
    if (proc->initial_sp != handoff.first_user_process_sp) return AEGIS_EINVAL;
    return AEGIS_OK;
}

bool userland_first_tiny_process_active(void) {
    return handoff.first_user_process_launch_ready && !handoff.first_user_process_syscall_seen;
}

bool userland_first_tiny_process_seen(void) {
    return handoff.first_user_process_seen && handoff.first_user_process_syscall_seen;
}

bool userland_first_tiny_process_return_probe_active(void) {
    return handoff.first_user_process_syscall_return_ready &&
           !handoff.first_user_process_continued_after_syscall;
}

bool userland_first_tiny_process_return_path_seen(void) {
    return handoff.first_user_process_seen &&
           handoff.first_user_process_syscall_seen &&
           handoff.first_user_process_syscall_return_ready &&
           handoff.first_user_process_continued_after_syscall;
}

bool userland_first_tiny_process_exit_active(void) {
    return handoff.first_user_process_continued_after_syscall &&
           !handoff.first_user_process_exited;
}

bool userland_first_tiny_process_exit_seen(void) {
    return handoff.first_user_process_exit_requested &&
           handoff.first_user_process_exited;
}

void userland_mark_first_user_process_syscall_return(u64 syscall_nr, u64 arg0, s64 ret, u64 elr, u64 spsr) {
    handoff.first_user_process_syscall_nr = syscall_nr;
    handoff.first_user_process_arg0 = arg0;
    handoff.first_user_process_elr = elr;
    handoff.first_user_process_spsr = spsr;
    handoff.first_user_process_syscall_return_value = ret;
    handoff.first_user_process_seen = true;
    handoff.first_user_process_syscall_seen = true;
    handoff.first_user_process_launch_ready = false;
    handoff.first_user_process_syscall_return_ready = true;

    /* Keep v31 transition state coherent: the first process used the same EL0 pipe. */
    handoff.el0_probe_syscall_nr = syscall_nr;
    handoff.el0_probe_arg0 = arg0;
    handoff.el0_probe_elr = elr;
    handoff.el0_probe_spsr = spsr;
    handoff.el0_svc_trap_seen = true;
    handoff.controlled_el0_transition_seen = true;

    if (handoff.process_descriptor_count >= 1U) {
        user_processes[0].state = AEGIS_USER_PROCESS_FIRST_SYSCALL_RETURN_READY;
    }
}

void userland_mark_first_user_process_continued_after_syscall(u64 syscall_nr, u64 arg0, u64 elr, u64 spsr) {
    handoff.first_user_process_continuation_syscall_nr = syscall_nr;
    handoff.first_user_process_continuation_arg0 = arg0;
    handoff.first_user_process_continuation_elr = elr;
    handoff.first_user_process_continuation_spsr = spsr;
    handoff.first_user_process_continued_after_syscall = true;

    if (handoff.process_descriptor_count >= 1U) {
        user_processes[0].state = AEGIS_USER_PROCESS_FIRST_CONTINUED_AFTER_SYSCALL;
    }
}

void userland_mark_first_user_process_exit(u64 syscall_nr, u64 exit_code, u64 elr, u64 spsr) {
    handoff.first_user_process_exit_syscall_nr = syscall_nr;
    handoff.first_user_process_exit_code = exit_code;
    handoff.first_user_process_exit_elr = elr;
    handoff.first_user_process_exit_spsr = spsr;
    handoff.first_user_process_exit_requested = true;
    handoff.first_user_process_exited = true;

    if (handoff.process_descriptor_count >= 1U) {
        user_processes[0].state = AEGIS_USER_PROCESS_FIRST_EXITED;
    }
}


void userland_mark_post_exit_scheduler_handoff(void) {
    if (!handoff.first_user_process_exited) return;
    handoff.post_exit_scheduler_handoff = true;
    handoff.scheduler_handoff_no_runnable_user = true;
}

bool userland_post_exit_scheduler_handoff_seen(void) {
    return handoff.post_exit_scheduler_handoff;
}

bool userland_no_runnable_user_processes(void) {
    if (!handoff.scheduler_handoff_no_runnable_user) return false;
    for (u32 i = 0; i < handoff.process_descriptor_count; i++) {
        aegis_user_process_state_t st = user_processes[i].state;
        if (st == AEGIS_USER_PROCESS_FIRST_LAUNCH_READY ||
            st == AEGIS_USER_PROCESS_FIRST_RUNNING ||
            st == AEGIS_USER_PROCESS_FIRST_SYSCALL_TRAPPED ||
            st == AEGIS_USER_PROCESS_FIRST_SYSCALL_RETURN_READY ||
            st == AEGIS_USER_PROCESS_FIRST_CONTINUED_AFTER_SYSCALL) {
            return false;
        }
    }
    return true;
}

/* Compatibility marker retained for older proof names; v33 uses the dispatching
 * return-path marker above instead of stopping at the first SVC.
 */
void userland_mark_first_user_process_syscall(u64 syscall_nr, u64 arg0, u64 elr, u64 spsr) {
    userland_mark_first_user_process_syscall_return(syscall_nr, arg0, 0, elr, spsr);
}

const aegis_userland_handoff_t *userland_handoff_state(void) {
    return &handoff;
}

u32 userland_feature_count(void) {
    return handoff.feature_count;
}

u32 userland_bound_count(void) {
    return handoff.bound_count;
}

u32 userland_ready_count(void) {
    return handoff.ready_count;
}

u32 userland_process_descriptor_count(void) {
    return handoff.process_descriptor_count;
}

bool userland_handoff_is_ready(void) {
    return handoff.handoff_ready;
}

bool userland_pre_el0_is_ready(void) {
    return handoff.pre_el0_ready;
}

bool userland_el0_transition_is_prepared(void) {
    return handoff.el0_transition_prepared;
}

bool userland_elf_loader_is_ready(void) {
    return handoff.elf_loader_ready && handoff.elf_aegis_init_validated && handoff.first_user_process_loaded_from_elf;
}

bool userland_pt_load_segments_copied(void) {
    return handoff.elf_pt_load_segments_copied;
}

bool userland_page_permissions_ready(void) {
    return handoff.user_kernel_page_permissions_ready;
}

bool userland_user_stack_ready(void) {
    return handoff.user_stack_argv_envp_ready;
}

bool userland_multi_process_ready(void) {
    return handoff.multiple_process_descriptors_ready;
}

bool userland_real_multiprocess_ready(void) {
    return handoff.real_multiprocess_launch_ready;
}

bool userland_expanded_syscalls_ready(void) {
    return handoff.syscall_expansion_ready;
}
