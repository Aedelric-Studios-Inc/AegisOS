/* SPDX-License-Identifier: Proprietary */
#pragma once
/* AegisOS early userland handoff model.
 *
 * v30 scope: keep EL0 launch deferred, but prove the pre-EL0 contract:
 *   - fake first user process descriptor
 *   - user address-space and stack layout
 *   - syscall ABI surface in kernel-safe mode
 *
 * v31.0 scope: controlled EL0 transition probe.  This validates that the
 * kernel can eret into an EL0 probe stub and receive an SVC trap back through
 * the EL0 synchronous exception vector.
 *
 * v32 scope: first tiny user process.  This promotes the aegis-init descriptor
 * from pre-EL0 contract into a launchable first process record, enters a tiny
 * EL0 userspace body, and proves one deliberate syscall trap back into EL1.
 *
 * v33 scope: syscall return path.  The kernel dispatches the first tiny
 * process SYS_GETPID request, writes the result into the saved EL0 x0 slot,
 * eret returns to EL0 after the SVC, and the user body proves continuation.
 *
 * v34 scope: tiny user process exit.  After continuing from SYS_GETPID, the
 * same tiny EL0 body issues SYS_EXIT; the kernel records the exit request,
 * marks the descriptor exited, and deliberately does not return to dead EL0.
 *
* v35 scope: closes this first-user-process bring-up section by proving a
 * post-exit scheduler handoff boundary and validating/binding the first
 * /sbin/aegis-init ELF64/AArch64 image contract before EL0 launch.
 *
 * v36 scope: open/read /sbin/aegis-init from initramfs through VFS and bind the
 * file-backed ELF image.
 *
 * v37 scope: prove copied PT_LOAD bytes and user/kernel page permission
 * metadata before launching the first file-backed user process.
 *
 * v38 scope: build an argv/envp user-stack bootstrap contract and create
 * multiple process descriptors for the first userspace service set.
 *
 * v42 scope: promote those descriptors into a real multi-process launch
 * contract and expand the syscall surface for process/service control.
 *
 * v43 scope: keep process execution stable while proving IPC/service messaging
 * mailboxes and a SYS_SEND_MSG/SYS_RECV_MSG roundtrip between early services.
 */

#include "types.h"

#define AEGIS_USERLAND_FEATURE_MAX      24
#define AEGIS_USERLAND_NAME_MAX         32
#define AEGIS_USERLAND_PATH_MAX         64
#define AEGIS_USERLAND_REQUIRED_SYSCALLS 6
#define AEGIS_USERLAND_PROCESS_MAX       8
#define AEGIS_USER_ARGV_MAX               4
#define AEGIS_USER_ENVP_MAX               4
#define AEGIS_USER_ARG_STRING_MAX         64
#define AEGIS_USER_STACK_BOOTSTRAP_SIZE   512UL
#define AEGIS_USER_PROCESS_STRIDE         0x0000000001000000UL
#define AEGIS_USER_STACK_STRIDE           0x0000000000020000UL

#define AEGIS_USER_TEXT_BASE        0x0000000000400000UL
#define AEGIS_USER_TEXT_SIZE        0x0000000000200000UL
#define AEGIS_USER_HEAP_BASE        0x0000000010000000UL
#define AEGIS_USER_HEAP_SIZE        0x0000000001000000UL
#define AEGIS_USER_IPC_BASE         0x0000000020000000UL
#define AEGIS_USER_IPC_SIZE         0x0000000000100000UL
#define AEGIS_USER_STACK_TOP        0x0000007fff000000UL
#define AEGIS_USER_STACK_SIZE       0x0000000000010000UL
#define AEGIS_USER_SYSCALL_GATE     0x0000007fffff0000UL

#define AEGIS_USER_REGION_READ      (1u << 0)
#define AEGIS_USER_REGION_WRITE     (1u << 1)
#define AEGIS_USER_REGION_EXEC      (1u << 2)
#define AEGIS_USER_REGION_USER      (1u << 3)
#define AEGIS_USER_REGION_GUARD     (1u << 4)
#define AEGIS_USER_REGION_KERNEL    (1u << 5)
#define AEGIS_USER_REGION_MAPPED    (1u << 6)

typedef enum aegis_userland_state {
    AEGIS_USERLAND_EMPTY = 0,
    AEGIS_USERLAND_REGISTERED,
    AEGIS_USERLAND_BOUND,
    AEGIS_USERLAND_HANDOFF_READY,
    AEGIS_USERLAND_FAILED,
} aegis_userland_state_t;

typedef enum aegis_userland_flags {
    AEGIS_USERLAND_FLAG_REQUIRED       = 1u << 0,
    AEGIS_USERLAND_FLAG_KERNEL_BACKED  = 1u << 1,
    AEGIS_USERLAND_FLAG_PROCESS        = 1u << 2,
    AEGIS_USERLAND_FLAG_CONTROL_PLANE  = 1u << 3,
} aegis_userland_flags_t;

typedef enum aegis_user_process_state {
    AEGIS_USER_PROCESS_EMPTY = 0,
    AEGIS_USER_PROCESS_DESCRIPTOR_READY,
    AEGIS_USER_PROCESS_LAYOUT_READY,
    AEGIS_USER_PROCESS_SYSCALL_ABI_READY,
    AEGIS_USER_PROCESS_ELF_LOADED,
    AEGIS_USER_PROCESS_EL0_PROBE_READY,
    AEGIS_USER_PROCESS_EL0_ENTERED,
    AEGIS_USER_PROCESS_EL0_SVC_TRAPPED,
    AEGIS_USER_PROCESS_FIRST_LAUNCH_READY,
    AEGIS_USER_PROCESS_FIRST_RUNNING,
    AEGIS_USER_PROCESS_FIRST_SYSCALL_TRAPPED,
    AEGIS_USER_PROCESS_FIRST_SYSCALL_RETURN_READY,
    AEGIS_USER_PROCESS_FIRST_CONTINUED_AFTER_SYSCALL,
    AEGIS_USER_PROCESS_ARGV_ENVP_READY,
    AEGIS_USER_PROCESS_SECONDARY_DESCRIPTOR_READY,
    AEGIS_USER_PROCESS_MULTIPROCESS_LAUNCH_READY,
    AEGIS_USER_PROCESS_FIRST_EXITED,
} aegis_user_process_state_t;

typedef struct aegis_user_region {
    virt_addr_t base;
    u64         size;
    u32         flags;
} aegis_user_region_t;

typedef struct aegis_user_process_descriptor {
    u32 id;
    u32 feature_id;
    u32 service_id;
    char name[AEGIS_USERLAND_NAME_MAX];
    char image_path[AEGIS_USERLAND_PATH_MAX];
    aegis_user_process_state_t state;
    virt_addr_t entry;
    aegis_user_region_t text;
    aegis_user_region_t heap;
    aegis_user_region_t ipc;
    aegis_user_region_t stack;
    aegis_user_region_t stack_guard;
    virt_addr_t syscall_gate;
    u64 initial_sp;
    u32 argc;
    u32 envc;
    virt_addr_t argv_user;
    virt_addr_t envp_user;
    virt_addr_t auxv_user;
    virt_addr_t bootstrap_sp;
    u64 bootstrap_stack_bytes;
    char argv[AEGIS_USER_ARGV_MAX][AEGIS_USER_ARG_STRING_MAX];
    char envp[AEGIS_USER_ENVP_MAX][AEGIS_USER_ARG_STRING_MAX];
} aegis_user_process_descriptor_t;

typedef struct aegis_userland_feature {
    u32 id;
    char name[AEGIS_USERLAND_NAME_MAX];
    char service_name[AEGIS_USERLAND_NAME_MAX];
    char image_path[AEGIS_USERLAND_PATH_MAX];
    aegis_userland_state_t state;
    u32 flags;
    u32 service_id;
} aegis_userland_feature_t;

typedef struct aegis_userland_handoff {
    bool initialised;
    bool feature_catalogue_ready;
    bool services_bound;
    bool syscall_surface_ready;
    bool handoff_ready;
    bool fake_process_descriptor_ready;
    bool user_stack_layout_ready;
    bool syscall_abi_kernel_safe_ready;
    bool pre_el0_ready;
    bool el0_transition_prepared;
    bool el0_entry_selected;
    bool el0_stack_selected;
    bool el0_svc_trap_seen;
    bool controlled_el0_transition_seen;
    bool first_user_process_descriptor_ready;
    bool first_user_process_launch_ready;
    bool first_user_process_seen;
    bool first_user_process_syscall_seen;
    bool first_user_process_syscall_return_ready;
    bool first_user_process_continued_after_syscall;
    bool first_user_process_exit_requested;
    bool first_user_process_exited;
    bool elf_loader_ready;
    bool elf_aegis_init_validated;
    bool first_user_process_loaded_from_elf;
    bool elf_pt_load_segments_copied;
    bool user_kernel_page_permissions_ready;
    bool user_stack_argv_envp_ready;
    bool multiple_process_descriptors_ready;
    bool real_multiprocess_launch_ready;
    bool syscall_expansion_ready;
    bool post_exit_scheduler_handoff;
    bool scheduler_handoff_no_runnable_user;
    u64  el0_probe_entry;
    u64  el0_probe_sp;
    u64  el0_probe_syscall_nr;
    u64  el0_probe_arg0;
    u64  el0_probe_elr;
    u64  el0_probe_spsr;
    u64  first_user_process_entry;
    u64  first_user_process_sp;
    u64  first_user_process_syscall_nr;
    u64  first_user_process_arg0;
    u64  first_user_process_elr;
    u64  first_user_process_spsr;
    s64  first_user_process_syscall_return_value;
    u64  first_user_process_continuation_syscall_nr;
    u64  first_user_process_continuation_arg0;
    u64  first_user_process_continuation_elr;
    u64  first_user_process_continuation_spsr;
    u64  first_user_process_exit_syscall_nr;
    u64  first_user_process_exit_code;
    u64  first_user_process_exit_elr;
    u64  first_user_process_exit_spsr;
    u64  elf_entry;
    u64  elf_text_base;
    u64  elf_text_size;
    u64  elf_text_file_bytes_copied;
    u64  elf_text_zero_bytes;
    u32  elf_text_region_flags;
    u32  bootstrap_argc;
    u32  bootstrap_envc;
    u32  secondary_process_descriptor_count;
    u32  launchable_process_descriptor_count;
    u32  scheduled_user_process_count;
    u32  syscall_expanded_count;
    u32  spawn_request_count;
    u32  last_spawn_pid;
    u64  bootstrap_stack_pointer;
    u64  bootstrap_argv_user;
    u64  bootstrap_envp_user;
    u64  bootstrap_auxv_user;
    u32  first_user_process_pid;
    u32 feature_count;
    u32 ready_count;
    u32 bound_count;
    u32 process_descriptor_count;
} aegis_userland_handoff_t;

void userland_handoff_init(void);
int  userland_connect_builtin_features(void);
int  userland_prepare_handoff(void);
int  userland_prepare_pre_el0_contract(void);
int  userland_selftest(void);
int  userland_pre_el0_selftest(void);
int  userland_prepare_controlled_el0_transition(u64 entry, u64 user_sp);
int  userland_controlled_el0_transition_selftest(void);
bool userland_controlled_el0_transition_active(void);
bool userland_controlled_el0_transition_seen(void);
void userland_mark_el0_svc_trap(u64 syscall_nr, u64 arg0, u64 elr, u64 spsr);

int  userland_bind_first_process_to_elf(const char *path, u64 entry, u64 text_base, u64 text_size);
int  userland_bind_first_process_to_loaded_elf(const char *path, u64 entry,
                                               u64 text_base, u64 text_size,
                                               u32 text_flags,
                                               u64 file_bytes_copied,
                                               u64 zero_bytes);
int  userland_first_process_elf_loader_selftest(void);
int  userland_pt_load_permissions_selftest(void);
int  userland_prepare_user_stack_bootstrap(void);
int  userland_user_stack_bootstrap_selftest(void);
int  userland_create_multiple_process_descriptors(void);
int  userland_multiple_process_descriptors_selftest(void);
bool userland_user_stack_bootstrap_ready(void);
bool userland_multiple_process_descriptors_ready(void);
int  userland_prepare_real_multiprocess_launch_path(void);
int  userland_real_multiprocess_launch_selftest(void);
int  userland_prepare_syscall_expansion(void);
int  userland_syscall_expansion_selftest(void);
bool userland_real_multiprocess_launch_ready(void);
bool userland_syscall_expansion_ready(void);
s64  userland_request_process_launch_by_path(const char *path, u64 flags);
u32  userland_feature_id_by_name(const char *name);
int  userland_prepare_first_tiny_user_process(u64 entry, u64 user_sp);
int  userland_first_tiny_user_process_selftest(void);
bool userland_first_tiny_process_active(void);
bool userland_first_tiny_process_seen(void);
bool userland_first_tiny_process_return_probe_active(void);
bool userland_first_tiny_process_return_path_seen(void);
bool userland_first_tiny_process_exit_active(void);
bool userland_first_tiny_process_exit_seen(void);
void userland_mark_first_user_process_exit(u64 syscall_nr, u64 exit_code, u64 elr, u64 spsr);
void userland_mark_post_exit_scheduler_handoff(void);
bool userland_post_exit_scheduler_handoff_seen(void);
bool userland_no_runnable_user_processes(void);
void userland_mark_first_user_process_syscall_return(u64 syscall_nr, u64 arg0, s64 ret, u64 elr, u64 spsr);
void userland_mark_first_user_process_continued_after_syscall(u64 syscall_nr, u64 arg0, u64 elr, u64 spsr);
void userland_mark_first_user_process_syscall(u64 syscall_nr, u64 arg0, u64 elr, u64 spsr);

const aegis_userland_handoff_t *userland_handoff_state(void);
const aegis_userland_feature_t *userland_find_feature(const char *name);
const aegis_userland_feature_t *userland_get_feature(u32 id);
const aegis_user_process_descriptor_t *userland_first_process_descriptor(void);
const aegis_user_process_descriptor_t *userland_process_descriptor(u32 index);

u32 userland_feature_count(void);
u32 userland_bound_count(void);
u32 userland_ready_count(void);
u32 userland_process_descriptor_count(void);
bool userland_handoff_is_ready(void);
bool userland_pre_el0_is_ready(void);
bool userland_el0_transition_is_prepared(void);
bool userland_elf_loader_is_ready(void);
bool userland_pt_load_segments_copied(void);
bool userland_page_permissions_ready(void);
bool userland_user_stack_ready(void);
bool userland_multi_process_ready(void);
bool userland_real_multiprocess_ready(void);
bool userland_expanded_syscalls_ready(void);
const char *userland_state_name(aegis_userland_state_t state);
const char *user_process_state_name(aegis_user_process_state_t state);
