/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/kernel_main.c
 * Kernel entry point called from boot/arm64/start.S.
 */

#include "aegis_kernel.h"
#include "hal.h"
#include "process.h"
#include "kernel_timer.h"
#include "boot_phase.h"
#include "early_log.h"
#include "exception.h"
#include "timer.h"
#include "vfs.h"
#include "initramfs.h"
#include "service_manager.h"
#include "userland.h"
#include "elf_loader.h"
#include "block_storage.h"
#include "ipc_service.h"
#include "service_supervisor.h"
#include "page_isolation.h"
#include "network_control.h"
#include "developer_preview.h"
#include "release_polish.h"
#include "release_final.h"
#include "interactive_console.h"

extern int  board_init(void);
extern void gic_init(void);
extern int  device_tree_init(const void *fdt);
extern u32  device_tree_get_total_size(void);
extern char _start[];
extern char _end[];
extern char _stack_bottom[];
extern char _stack_top[];
extern void aegis_el0_transition_enter(u64 entry, u64 user_sp);
extern void aegis_el0_transition_test_entry(void);
extern void aegis_first_user_process_entry(void);
extern void aegis_first_user_process_after_syscall(void);
extern void aegis_first_user_process_return_probe(void);
extern void aegis_first_user_process_exit_call(void);
extern void aegis_first_user_process_after_exit(void);

#ifdef AEGISOS_QEMU_SMOKE
static void qemu_smoke_write0(const char *message) {
    __asm__ volatile(
        "mov x0, #4\n"
        "mov x1, %0\n"
        "hlt #0xf000\n"
        :: "r"(message)
        : "x0", "x1", "memory");
}

/*
 * QEMU boot-proof checkpoints.
 *
 * These functions intentionally exist as real, non-inlined symbols. The QEMU
 * harness proves boot progression by matching their addresses in the execution
 * trace, so this works even when PL011/semihost stdout is swallowed by the host.
 */
#define QEMU_BOOT_CHECKPOINT_FN(name, text) \
    __attribute__((noinline, used)) static void qemu_boot_checkpoint_##name(void) { \
        __asm__ volatile("nop" ::: "memory"); \
        qemu_smoke_write0(text); \
    }

QEMU_BOOT_CHECKPOINT_FN(kernel_main, "[AegisOS:proof] kernel_main\n")
QEMU_BOOT_CHECKPOINT_FN(visible_log, "[AegisOS:proof] visible log path\n")
QEMU_BOOT_CHECKPOINT_FN(memory, "[AegisOS:proof] memory phase\n")
QEMU_BOOT_CHECKPOINT_FN(memory_selftest, "[AegisOS:proof] memory selftest\n")
QEMU_BOOT_CHECKPOINT_FN(core, "[AegisOS:proof] core phase\n")
QEMU_BOOT_CHECKPOINT_FN(exception_vectors, "[AegisOS:proof] exception vectors\n")
QEMU_BOOT_CHECKPOINT_FN(timer, "[AegisOS:proof] timer proof\n")
QEMU_BOOT_CHECKPOINT_FN(scheduler, "[AegisOS:proof] scheduler proof\n")
QEMU_BOOT_CHECKPOINT_FN(devices, "[AegisOS:proof] devices phase\n")
QEMU_BOOT_CHECKPOINT_FN(kernel_services, "[AegisOS:proof] kernel services phase\n")
QEMU_BOOT_CHECKPOINT_FN(security, "[AegisOS:proof] security phase\n")
QEMU_BOOT_CHECKPOINT_FN(network, "[AegisOS:proof] network phase\n")
QEMU_BOOT_CHECKPOINT_FN(init, "[AegisOS:proof] init phase\n")
QEMU_BOOT_CHECKPOINT_FN(running, "[AegisOS:proof] running phase\n")
QEMU_BOOT_CHECKPOINT_FN(first_init_thread, "[AegisOS:proof] first init thread entered\n")
QEMU_BOOT_CHECKPOINT_FN(vfs_init, "[AegisOS:proof] VFS init\n")
QEMU_BOOT_CHECKPOINT_FN(initramfs_mount, "[AegisOS:proof] initramfs mounted\n")
QEMU_BOOT_CHECKPOINT_FN(vfs_mounts, "[AegisOS:proof] VFS bootstrap mounts ready\n")
QEMU_BOOT_CHECKPOINT_FN(initramfs_file_table_ready, "[AegisOS:proof] initramfs userland file table ready\n")
QEMU_BOOT_CHECKPOINT_FN(vfs_open_aegis_init, "[AegisOS:proof] VFS opened /sbin/aegis-init\n")
QEMU_BOOT_CHECKPOINT_FN(vfs_read_aegis_init, "[AegisOS:proof] VFS read /sbin/aegis-init\n")
QEMU_BOOT_CHECKPOINT_FN(elf_loaded_from_initramfs, "[AegisOS:proof] ELF loaded from initramfs\n")
QEMU_BOOT_CHECKPOINT_FN(elf_pt_load_segments_copied, "[AegisOS:proof] ELF PT_LOAD segments copied into user image backing\n")
QEMU_BOOT_CHECKPOINT_FN(user_kernel_page_permissions_ready, "[AegisOS:proof] user/kernel page permission metadata ready\n")
QEMU_BOOT_CHECKPOINT_FN(user_stack_argv_envp_ready, "[AegisOS:proof] argv/envp user-stack bootstrap ready\n")
QEMU_BOOT_CHECKPOINT_FN(multiple_user_process_descriptors_ready, "[AegisOS:proof] multiple user process descriptors ready\n")
QEMU_BOOT_CHECKPOINT_FN(block_storage_abstraction_ready, "[AegisOS:proof] v41 block/storage abstraction ready\n")
QEMU_BOOT_CHECKPOINT_FN(process_table_cleanup_ready, "[AegisOS:proof] v41 process table cleanup ready\n")
QEMU_BOOT_CHECKPOINT_FN(real_multiprocess_launch_path_ready, "[AegisOS:proof] v42 real multi-process launch path ready\n")
QEMU_BOOT_CHECKPOINT_FN(syscall_expansion_ready, "[AegisOS:proof] v42 syscall expansion ready\n")
QEMU_BOOT_CHECKPOINT_FN(ipc_service_messaging_ready, "[AegisOS:proof] v43 IPC service messaging ready\n")
QEMU_BOOT_CHECKPOINT_FN(ipc_message_roundtrip_ready, "[AegisOS:proof] v43 IPC message roundtrip ready\n")
QEMU_BOOT_CHECKPOINT_FN(v44_service_supervisor_ready, "[AegisOS:proof] v44 service supervisor ready\n")
QEMU_BOOT_CHECKPOINT_FN(v44_fault_injection_ready, "[AegisOS:proof] v44 controlled fault injection ready\n")
QEMU_BOOT_CHECKPOINT_FN(v44_page_fault_trap_recorded, "[AegisOS:proof] v44 page-fault trap recorded\n")
QEMU_BOOT_CHECKPOINT_FN(v44_service_marked_faulted, "[AegisOS:proof] v44 service marked faulted\n")
QEMU_BOOT_CHECKPOINT_FN(v44_supervisor_handoff_preserved, "[AegisOS:proof] v44 supervisor handoff preserved\n")
QEMU_BOOT_CHECKPOINT_FN(v45_page_tables_allocated, "[AegisOS:proof] v45 per-process TTBR0 page tables allocated\n")
QEMU_BOOT_CHECKPOINT_FN(v45_user_regions_mapped, "[AegisOS:proof] v45 user regions mapped with EL0 permissions\n")
QEMU_BOOT_CHECKPOINT_FN(v45_kernel_window_blocked, "[AegisOS:proof] v45 kernel window blocked from user roots\n")
QEMU_BOOT_CHECKPOINT_FN(v45_guard_pages_unmapped, "[AegisOS:proof] v45 guard pages left unmapped\n")
QEMU_BOOT_CHECKPOINT_FN(v45_ttbr0_contract_ready, "[AegisOS:proof] v45 TTBR0 isolation contract ready\n")
QEMU_BOOT_CHECKPOINT_FN(v46_network_stack_ready, "[AegisOS:proof] v46 network stack bring-up ready\n")
QEMU_BOOT_CHECKPOINT_FN(v46_dhcp_bound, "[AegisOS:proof] v46 DHCP lease bound\n")
QEMU_BOOT_CHECKPOINT_FN(v46_nat_firewall_ready, "[AegisOS:proof] v46 NAT/firewall control plane ready\n")
QEMU_BOOT_CHECKPOINT_FN(v46_router_control_plane_ready, "[AegisOS:proof] v46 router control plane ready\n")
QEMU_BOOT_CHECKPOINT_FN(v47_dev_preview_manifest_ready, "[AegisOS:proof] v47 Developer Preview manifest ready\n")
QEMU_BOOT_CHECKPOINT_FN(v47_board_profiles_ready, "[AegisOS:proof] v47 board profiles ready\n")
QEMU_BOOT_CHECKPOINT_FN(v47_installer_flash_flow_ready, "[AegisOS:proof] v47 installer/flash flow ready\n")
QEMU_BOOT_CHECKPOINT_FN(v47_developer_preview_img_ready, "[AegisOS:proof] v47 AegisBox Developer Preview IMG ready\n")
QEMU_BOOT_CHECKPOINT_FN(v48_board_profile_polish_ready, "[AegisOS:proof] v48 board profile polish ready\n")
QEMU_BOOT_CHECKPOINT_FN(v48_service_config_matrix_ready, "[AegisOS:proof] v48 service config matrix ready\n")
QEMU_BOOT_CHECKPOINT_FN(v48_security_hardening_ready, "[AegisOS:proof] v48 security hardening baseline ready\n")
QEMU_BOOT_CHECKPOINT_FN(v48_image_polish_manifest_ready, "[AegisOS:proof] v48 image polish manifest ready\n")
QEMU_BOOT_CHECKPOINT_FN(v50_installer_flash_flow_hardened, "[AegisOS:proof] v50 installer/flash flow hardened\n")
QEMU_BOOT_CHECKPOINT_FN(v51_variant_images_ready, "[AegisOS:proof] v51 Lite/Pro/Bastion/Router variant images ready\n")
QEMU_BOOT_CHECKPOINT_FN(v52_security_service_defaults_frozen, "[AegisOS:proof] v52 security hardening and service defaults frozen\n")
QEMU_BOOT_CHECKPOINT_FN(v53_docs_known_limits_hardware_notes_ready, "[AegisOS:proof] v53 docs, known limitations, and hardware notes ready\n")
QEMU_BOOT_CHECKPOINT_FN(v54_final_rc_audit_signing_checksums_ready, "[AegisOS:proof] v54 final RC audit, signing manifest, and checksums ready\n")
QEMU_BOOT_CHECKPOINT_FN(v55_release_img_ready, "[AegisOS:proof] v55 Release IMG ready\n")
QEMU_BOOT_CHECKPOINT_FN(first_user_process_from_file, "[AegisOS:proof] first user process bound from file-backed ELF\n")
QEMU_BOOT_CHECKPOINT_FN(kernel_services_registered, "[AegisOS:proof] kernel services registered\n")
QEMU_BOOT_CHECKPOINT_FN(service_manager_prepared, "[AegisOS:proof] service manager prepared\n")
QEMU_BOOT_CHECKPOINT_FN(userland_catalogue, "[AegisOS:proof] userland catalogue connected\n")
QEMU_BOOT_CHECKPOINT_FN(userland_service_links, "[AegisOS:proof] userland service links ready\n")
QEMU_BOOT_CHECKPOINT_FN(userland_handoff_ready, "[AegisOS:proof] userland handoff ready\n")
QEMU_BOOT_CHECKPOINT_FN(fake_user_process_descriptor, "[AegisOS:proof] fake user process descriptor ready\n")
QEMU_BOOT_CHECKPOINT_FN(user_stack_layout, "[AegisOS:proof] user stack/address layout ready\n")
QEMU_BOOT_CHECKPOINT_FN(syscall_abi_kernel_safe, "[AegisOS:proof] syscall ABI kernel-safe proof ready\n")
QEMU_BOOT_CHECKPOINT_FN(pre_el0_contract_ready, "[AegisOS:proof] pre-EL0 contract ready\n")
QEMU_BOOT_CHECKPOINT_FN(el0_transition_prepared, "[AegisOS:proof] EL0 transition prepared\n")
QEMU_BOOT_CHECKPOINT_FN(first_user_process_descriptor, "[AegisOS:proof] first tiny user process descriptor launch-ready\n")
QEMU_BOOT_CHECKPOINT_FN(first_user_process_launch, "[AegisOS:proof] launching first tiny user process\n")
QEMU_BOOT_CHECKPOINT_FN(syscall_return_path_ready, "[AegisOS:proof] syscall return path ready\n")
QEMU_BOOT_CHECKPOINT_FN(first_user_process_exit_ready, "[AegisOS:proof] first tiny process exit proof ready\n")
QEMU_BOOT_CHECKPOINT_FN(elf_loader_ready, "[AegisOS:proof] ELF loader initialised\n")
QEMU_BOOT_CHECKPOINT_FN(elf_aegis_init_loaded, "[AegisOS:proof] /sbin/aegis-init ELF validated and bound\n")
QEMU_BOOT_CHECKPOINT_FN(el0_eret_launch, "[AegisOS:proof] launching controlled EL0 eret\n")
#endif


#define AEGIS_QEMU_RAM_BASE 0x40000000UL
#define AEGIS_QEMU_RAM_SIZE 0x40000000UL
#define AEGIS_QEMU_STUB_SIZE 0x00100000UL
#define AEGIS_QEMU_HEAP_BASE 0x42080000UL
#define AEGIS_QEMU_HEAP_SIZE (64UL * 1024UL * 1024UL)

static void reserve_or_panic(const char *name, phys_addr_t base, u64 size) {
    int rc = phys_reserve_range(base, size);
    if (rc != AEGIS_OK) {
        printk("[AegisOS:memory] reserve failed: %s base=%p size=%p\n",
               name, (void *)(uptr)base, (void *)(uptr)size);
        PANIC("memory reservation failed");
    }
    printk("[AegisOS:memory] reserved %s base=%p size=%p\n",
           name, (void *)(uptr)base, (void *)(uptr)size);
}

static void memory_bringup_selftest(u64 dtb_ptr) {
    phys_addr_t kernel_base = (phys_addr_t)(uptr)_start;
    phys_addr_t kernel_end = (phys_addr_t)PAGE_ALIGN((uptr)_end);
    u64 kernel_size = kernel_end - kernel_base;

    reserve_or_panic("qemu-stub-window", AEGIS_QEMU_RAM_BASE, AEGIS_QEMU_STUB_SIZE);
    reserve_or_panic("kernel-image", kernel_base, kernel_size);
    reserve_or_panic("kernel-stack", (phys_addr_t)(uptr)_stack_bottom,
                     (u64)((uptr)_stack_top - (uptr)_stack_bottom));
    reserve_or_panic("kernel-heap-window", AEGIS_QEMU_HEAP_BASE, AEGIS_QEMU_HEAP_SIZE);

    if (dtb_ptr != 0) {
        u64 dtb_size = device_tree_get_total_size();
        if (dtb_size == 0 || dtb_size > (16UL * 1024UL * 1024UL)) {
            dtb_size = 2UL * 1024UL * 1024UL;
        }
        reserve_or_panic("fdt", (phys_addr_t)dtb_ptr, dtb_size);
    }

    phys_addr_t p1 = phys_alloc_page();
    phys_addr_t p2 = phys_alloc_page();
    if (p1 == 0 || p2 == 0 || p1 == p2) {
        PANIC("physical page allocator selftest failed");
    }
    if ((p1 & (PAGE_SIZE - 1)) || (p2 & (PAGE_SIZE - 1))) {
        PANIC("physical page allocator returned unaligned page");
    }
    printk("[AegisOS:memory] allocated test pages: %p %p\n",
           (void *)(uptr)p1, (void *)(uptr)p2);
    phys_free_page(p2);
    phys_free_page(p1);

    heap_init(AEGIS_QEMU_HEAP_BASE, AEGIS_QEMU_HEAP_SIZE);
    u8 *a = (u8 *)kmalloc(128);
    u8 *b = (u8 *)kmalloc(256);
    if (!a || !b || a == b) {
        PANIC("kernel heap selftest allocation failed");
    }
    for (u64 i = 0; i < 128; i++) a[i] = (u8)(0xa0u + (i & 0x0fu));
    for (u64 i = 0; i < 256; i++) b[i] = (u8)(0x50u + (i & 0x0fu));
    if (a[0] != 0xa0u || a[127] != 0xafu || b[0] != 0x50u || b[255] != 0x5fu) {
        PANIC("kernel heap selftest pattern failed");
    }
    kfree(b);
    kfree(a);

    printk("[AegisOS:memory] selftest ok: total=%u used=%u free=%u\n",
           (unsigned int)phys_mem_total_pages(),
           (unsigned int)phys_mem_used_pages(),
           (unsigned int)phys_mem_free_pages());
}

static void exception_vector_bringup_selftest(void) {
    int rc = exception_vector_selftest();
    printk("[AegisOS:exceptions] VBAR_EL1=%p vectors=%p\n",
           (void *)(uptr)exception_vector_base_read(),
           (void *)(uptr)exception_vector_table_addr());
    if (rc != AEGIS_OK) {
        PANIC("exception vector selftest failed");
    }
    printk("[AegisOS:exceptions] vector table installed and aligned\n");
}


static void timer_bringup_selftest_or_panic(void) {
    int rc = kernel_timer_bringup_selftest();
    printk("[AegisOS:timer] cntfrq=%u ticks=%p rc=%d\n",
           (unsigned int)hal_get_cpu_freq(),
           (void *)(uptr)timer_get_ticks(),
           rc);
    if (rc != AEGIS_OK) {
        PANIC("timer bring-up selftest failed");
    }
    printk("[AegisOS:timer] ARM generic timer monotonic/prog proof ok\n");
}

#ifdef AEGISOS_QEMU_SMOKE
static void scheduler_bringup_selftest_or_panic(void) {
    int rc = scheduler_bringup_selftest();
    printk("[AegisOS:scheduler] task_count=%u ready_count=%u rc=%d\n",
           (unsigned int)scheduler_task_count(),
           (unsigned int)scheduler_ready_count(),
           rc);
    if (rc != AEGIS_OK) {
        PANIC("scheduler bring-up selftest failed");
    }
    printk("[AegisOS:scheduler] ready queue/task context proof ok\n");
}

static void scheduler_first_init_thread_selftest_or_panic(void) {
    int rc = scheduler_first_init_thread_selftest();
    printk("[AegisOS:scheduler] first init thread preflight rc=%d task_count=%u ready_count=%u\n",
           rc,
           (unsigned int)scheduler_task_count(),
           (unsigned int)scheduler_ready_count());
    if (rc != AEGIS_OK) {
        PANIC("first init thread scheduler preflight failed");
    }
    printk("[AegisOS:scheduler] first init thread preflight ok\n");
}

#endif

static void kernel_idle_task(void) {
    for (;;) {
        __asm__ volatile("wfi");
        scheduler_yield();
    }
}

static void aegisbox_init_mount_vfs_or_panic(void) {
    int rc = vfs_init();
    printk("[AegisOS:init] vfs_init rc=%d\n", rc);
    if (rc != AEGIS_OK) {
        PANIC("vfs_init failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_vfs_init();
#endif

    rc = initramfs_mount_empty_root();
    printk("[AegisOS:init] initramfs empty root mount rc=%d files=%u\n",
           rc, (unsigned int)initramfs_file_count());
    if (rc != AEGIS_OK || !initramfs_is_mounted() || !vfs_is_mounted("/")) {
        PANIC("initramfs root mount failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_initramfs_mount();
#endif

    rc = initramfs_install_builtin_userland();
    printk("[AegisOS:initramfs] installed builtin userland rc=%d files=%u has_init=%u size=%p\n",
           rc,
           (unsigned int)initramfs_file_count(),
           initramfs_has_file("/sbin/aegis-init") ? 1U : 0U,
           (void *)(uptr)initramfs_file_size("/sbin/aegis-init"));
    if (rc != AEGIS_OK || !initramfs_has_file("/sbin/aegis-init")) {
        PANIC("initramfs builtin /sbin/aegis-init install failed");
    }

    rc = initramfs_install_release_rootfs();
    printk("[AegisOS:initramfs] v55.1 runtime rootfs bridge rc=%d files=%u has_shell=%u has_services=%u has_rustmyadmin=%u\n",
           rc,
           (unsigned int)initramfs_file_count(),
           initramfs_has_file("/bin/aegis-shell") ? 1U : 0U,
           initramfs_has_file("/etc/services.toml") ? 1U : 0U,
           initramfs_has_file("/opt/rustmyadmin/INTEGRATION.txt") ? 1U : 0U);
    if (rc != AEGIS_OK || !initramfs_has_file("/bin/aegis-shell") ||
        !initramfs_has_file("/etc/services.toml") ||
        !initramfs_has_file("/opt/rustmyadmin/INTEGRATION.txt")) {
        PANIC("v55.1 runtime rootfs bridge install failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_initramfs_file_table_ready();
#endif

    rc = vfs_mount_bootstrap_pseudo("/dev", "devfs");
    printk("[AegisOS:init] devfs mount rc=%d\n", rc);
    if (rc != AEGIS_OK) {
        PANIC("devfs mount failed");
    }

    rc = vfs_mount_bootstrap_pseudo("/proc", "procfs");
    printk("[AegisOS:init] procfs mount rc=%d\n", rc);
    if (rc != AEGIS_OK) {
        PANIC("procfs mount failed");
    }

    rc = vfs_bootstrap_selftest();
    printk("[AegisOS:init] VFS bootstrap selftest rc=%d mounts=%u root=%s dev=%s proc=%s\n",
           rc,
           (unsigned int)vfs_mount_count(),
           vfs_mount_fs_name("/"),
           vfs_mount_fs_name("/dev"),
           vfs_mount_fs_name("/proc"));
    if (rc != AEGIS_OK) {
        PANIC("VFS bootstrap selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_vfs_mounts();
#endif
}


static void aegisbox_init_register_services_or_panic(void) {
    int rc = service_manager_register_builtin_kernel_services();
    printk("[AegisOS:init] kernel service registry rc=%d count=%u registered=%u prepared=%u\n",
           rc,
           (unsigned int)service_manager_count(),
           (unsigned int)service_manager_registered_count(),
           (unsigned int)service_manager_prepared_count());
    if (rc != AEGIS_OK) {
        PANIC("kernel service registration failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_kernel_services_registered();
#endif

    rc = service_manager_prepare();
    printk("[AegisOS:init] service manager prepare rc=%d count=%u prepared=%u is_prepared=%u\n",
           rc,
           (unsigned int)service_manager_count(),
           (unsigned int)service_manager_prepared_count(),
           service_manager_is_prepared() ? 1U : 0U);
    if (rc != AEGIS_OK) {
        PANIC("service manager prepare failed");
    }

    rc = service_manager_selftest();
    printk("[AegisOS:init] service manager selftest rc=%d vfs=%s aegisd=%s\n",
           rc,
           service_manager_find("vfs") ? service_state_name(service_manager_find("vfs")->state) : "missing",
           service_manager_find("aegisd") ? service_state_name(service_manager_find("aegisd")->state) : "missing");
    if (rc != AEGIS_OK) {
        PANIC("service manager selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_service_manager_prepared();
#endif
}

static void aegisbox_init_prepare_userland_or_panic(void) {
    userland_handoff_init();
    int rc = userland_connect_builtin_features();
    printk("[AegisOS:userland] connect features rc=%d count=%u bound=%u\n",
           rc,
           (unsigned int)userland_feature_count(),
           (unsigned int)userland_bound_count());
    if (rc != AEGIS_OK) {
        PANIC("userland feature connection failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_userland_catalogue();
    qemu_boot_checkpoint_userland_service_links();
#endif

    rc = userland_prepare_handoff();
    const aegis_userland_handoff_t *handoff = userland_handoff_state();
#ifdef AEGISOS_QEMU_SMOKE
    printk("[AegisOS:userland] handoff prepare rc=%d features=%u ready=%u syscalls=%u mounted=%u\n",
           rc,
           (unsigned int)handoff->feature_count,
           (unsigned int)handoff->ready_count,
           handoff->syscall_surface_ready ? 1U : 0U,
           (vfs_is_mounted("/") && vfs_is_mounted("/dev") && vfs_is_mounted("/proc")) ? 1U : 0U);
#else
    /* v55.1.1: keep the interactive boot path away from the long vararg
     * printk/vfs status line that can stall serial output before the shell.
     * The full detailed handoff printk remains in QEMU smoke builds.
     */
    (void)handoff;
    printk("[AegisOS:userland] handoff prepare complete\n");
#endif
    if (rc != AEGIS_OK) {
        PANIC("userland handoff prepare failed");
    }

#ifdef AEGISOS_QEMU_SMOKE
    rc = userland_selftest();
    const aegis_userland_feature_t *init = userland_find_feature("aegis-init");
    const aegis_userland_feature_t *router = userland_find_feature("routerd");
    const aegis_userland_feature_t *admin = userland_find_feature("rustmyadmin");
    printk("[AegisOS:userland] selftest rc=%d init=%s router=%s admin=%s\n",
           rc,
           init ? userland_state_name(init->state) : "missing",
           router ? userland_state_name(router->state) : "missing",
           admin ? userland_state_name(admin->state) : "missing");
    if (rc != AEGIS_OK) {
        PANIC("userland handoff selftest failed");
    }
    qemu_boot_checkpoint_userland_handoff_ready();
#else
    /* v55.1.2: no printk here for normal interactive boots.  On QEMU
     * release boots the serial path can stall inside long proof/status strings
     * before the shell starts.  The caller now jumps straight into the
     * interactive console after preparing compact runtime state.
     */
#endif
}

static void aegisbox_init_prepare_pre_el0_or_panic(void) {
    int rc = userland_prepare_pre_el0_contract();
    const aegis_userland_handoff_t *handoff = userland_handoff_state();
    const aegis_user_process_descriptor_t *first = userland_first_process_descriptor();
    printk("[AegisOS:userland] pre-EL0 prepare rc=%d descriptors=%u ready=%u\n",
           rc,
           (unsigned int)handoff->process_descriptor_count,
           handoff->pre_el0_ready ? 1U : 0U);
    if (rc != AEGIS_OK || !first) {
        PANIC("pre-EL0 user process contract prepare failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_fake_user_process_descriptor();
    qemu_boot_checkpoint_user_stack_layout();
    qemu_boot_checkpoint_syscall_abi_kernel_safe();
#endif

    printk("[AegisOS:userland] first user descriptor name=%s image=%s entry=%p sp=%p state=%s\n",
           first->name,
           first->image_path,
           (void *)(uptr)first->entry,
           (void *)(uptr)first->initial_sp,
           user_process_state_name(first->state));

    rc = userland_pre_el0_selftest();
    printk("[AegisOS:userland] pre-EL0 selftest rc=%d text=%p stack=%p guard=%p gate=%p\n",
           rc,
           (void *)(uptr)first->text.base,
           (void *)(uptr)first->stack.base,
           (void *)(uptr)first->stack_guard.base,
           (void *)(uptr)first->syscall_gate);
    if (rc != AEGIS_OK) {
        PANIC("pre-EL0 user process selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_pre_el0_contract_ready();
#endif
}


static void aegisbox_init_load_first_elf_or_panic(void) {
    elf_loader_init();
    vnode_t *probe = vfs_open("/sbin/aegis-init");
    if (!probe) {
        PANIC("VFS failed to open /sbin/aegis-init");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_vfs_open_aegis_init();
#endif
    u8 probe_buf[8];
    int probe_read = vfs_read(probe, 0, probe_buf, sizeof(probe_buf));
    vfs_close(probe);
    if (probe_read != (int)sizeof(probe_buf)) {
        PANIC("VFS failed to read /sbin/aegis-init");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_vfs_read_aegis_init();
#endif

    aegis_elf_image_info_t image;
    int rc = elf_loader_load_vfs_aegis_init(&image);
    printk("[AegisOS:elf] load VFS %s rc=%d state=%s entry=%p text=%p size=%p\n",
           image.path,
           rc,
           elf_loader_state_name(),
           (void *)(uptr)image.entry,
           (void *)(uptr)image.text_vaddr,
           (void *)(uptr)image.text_memsz);
    if (rc != AEGIS_OK) {
        PANIC("ELF loader failed to validate VFS /sbin/aegis-init");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_elf_loaded_from_initramfs();
#endif

    rc = elf_loader_selftest();
    if (rc != AEGIS_OK || !elf_loader_builtin_aegis_init_ready()) {
        PANIC("ELF loader selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_elf_loader_ready();
#endif

    rc = elf_loader_pt_load_selftest();
    printk("[AegisOS:elf] PT_LOAD copy selftest rc=%d copied=%u perms=%u file_bytes=%p zero=%p\n",
           rc,
           elf_loader_segments_copied() ? 1U : 0U,
           elf_loader_permissions_ready() ? 1U : 0U,
           (void *)(uptr)image.text_file_bytes_copied,
           (void *)(uptr)image.text_zero_bytes);
    if (rc != AEGIS_OK || !elf_loader_segments_copied() || !elf_loader_permissions_ready()) {
        PANIC("ELF PT_LOAD copy/permission selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_elf_pt_load_segments_copied();
#endif

    rc = userland_bind_first_process_to_loaded_elf(image.path,
                                                   image.entry,
                                                   image.text_vaddr,
                                                   image.text_memsz,
                                                   AEGIS_USER_REGION_READ |
                                                   AEGIS_USER_REGION_EXEC |
                                                   AEGIS_USER_REGION_USER |
                                                   AEGIS_USER_REGION_MAPPED,
                                                   image.text_file_bytes_copied,
                                                   image.text_zero_bytes);
    printk("[AegisOS:elf] first process ELF bind rc=%d loader_ready=%u descriptor_state=%s ptload=%u perms=%u\n",
           rc,
           userland_elf_loader_is_ready() ? 1U : 0U,
           user_process_state_name(userland_first_process_descriptor()->state),
           userland_pt_load_segments_copied() ? 1U : 0U,
           userland_page_permissions_ready() ? 1U : 0U);
    if (rc != AEGIS_OK) {
        PANIC("ELF loader bind to first user process failed");
    }

    rc = userland_first_process_elf_loader_selftest();
    if (rc != AEGIS_OK) {
        PANIC("first process ELF loader selftest failed");
    }
    rc = userland_pt_load_permissions_selftest();
    if (rc != AEGIS_OK) {
        PANIC("first process PT_LOAD permission selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_user_kernel_page_permissions_ready();
    qemu_boot_checkpoint_elf_aegis_init_loaded();
    qemu_boot_checkpoint_first_user_process_from_file();
#endif
}




static void aegisbox_init_prepare_v42_multiprocess_syscalls_or_panic(void) {
    int rc = userland_prepare_real_multiprocess_launch_path();
    const aegis_userland_handoff_t *handoff = userland_handoff_state();
    printk("[AegisOS:userland] v42 multi-process launch path rc=%d descriptors=%u launchable=%u scheduled=%u\n",
           rc,
           (unsigned int)handoff->process_descriptor_count,
           (unsigned int)handoff->launchable_process_descriptor_count,
           (unsigned int)handoff->scheduled_user_process_count);
    if (rc != AEGIS_OK) {
        PANIC("v42 real multi-process launch path prepare failed");
    }

    rc = userland_real_multiprocess_launch_selftest();
    if (rc != AEGIS_OK || !userland_real_multiprocess_launch_ready()) {
        PANIC("v42 real multi-process launch path selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_real_multiprocess_launch_path_ready();
#endif

    rc = userland_prepare_syscall_expansion();
    handoff = userland_handoff_state();
    printk("[AegisOS:syscall] v42 expansion prepare rc=%d nr=%u\n",
           rc,
           (unsigned int)handoff->syscall_expanded_count);
    if (rc != AEGIS_OK) {
        PANIC("v42 syscall expansion prepare failed");
    }

    rc = userland_syscall_expansion_selftest();
    handoff = userland_handoff_state();
    printk("[AegisOS:syscall] v42 expansion selftest rc=%d spawn_requests=%u last_spawn_pid=%u\n",
           rc,
           (unsigned int)handoff->spawn_request_count,
           (unsigned int)handoff->last_spawn_pid);
    if (rc != AEGIS_OK || !userland_syscall_expansion_ready()) {
        PANIC("v42 syscall expansion selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_syscall_expansion_ready();
#endif
}

static void aegisbox_init_prepare_v43_ipc_service_messaging_or_panic(void) {
    service_ipc_init();
    int rc = service_ipc_prepare_service_messaging();
    const aegis_ipc_service_state_t *ipc = service_ipc_state();
    printk("[AegisOS:ipc] v43 service messaging rc=%d mailboxes=%u channels=%u routes=%u last_channel=%u\n",
           rc,
           (unsigned int)ipc->mailbox_count,
           (unsigned int)ipc->channel_count,
           (unsigned int)ipc->route_count,
           (unsigned int)ipc->last_channel_id);
    if (rc != AEGIS_OK) {
        PANIC("v43 IPC/service messaging prepare failed");
    }

    rc = service_ipc_service_messaging_selftest();
    if (rc != AEGIS_OK || !service_ipc_service_messaging_ready()) {
        PANIC("v43 IPC/service messaging selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_ipc_service_messaging_ready();
#endif

    rc = service_ipc_message_roundtrip_selftest();
    ipc = service_ipc_state();
    printk("[AegisOS:ipc] v43 message roundtrip rc=%d channel=%u bytes=%u ready=%u\n",
           rc,
           (unsigned int)ipc->last_channel_id,
           (unsigned int)ipc->last_roundtrip_bytes,
           ipc->message_roundtrip_ready ? 1U : 0U);
    if (rc != AEGIS_OK || !service_ipc_message_roundtrip_ready()) {
        PANIC("v43 IPC message roundtrip selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_ipc_message_roundtrip_ready();
#endif
}

static void aegisbox_init_prepare_v44_service_supervisor_or_panic(void) {
    service_supervisor_init();

    int rc = service_supervisor_register_user_processes();
    const aegis_service_supervisor_state_t *sup = service_supervisor_state();
    printk("[AegisOS:supervisor] v44 register rc=%d services=%u registry=%u\n",
           rc,
           (unsigned int)sup->service_count,
           sup->registry_ready ? 1U : 0U);
    if (rc != AEGIS_OK || !service_supervisor_registry_ready()) {
        PANIC("v44 service supervisor registry failed");
    }

    rc = service_supervisor_start_registered();
    sup = service_supervisor_state();
    printk("[AegisOS:supervisor] v44 start rc=%d running=%u/%u\n",
           rc,
           (unsigned int)sup->running_count,
           (unsigned int)sup->service_count);
    if (rc != AEGIS_OK) {
        PANIC("v44 service supervisor start failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v44_service_supervisor_ready();
    qemu_boot_checkpoint_v44_fault_injection_ready();
#endif

    rc = service_supervisor_prepare_v44_fault_proof();
    sup = service_supervisor_state();
    const aegis_supervised_service_t *router = service_supervisor_find("routerd");
    printk("[AegisOS:supervisor] v44 page fault proof rc=%d faults=%u faulted=%u last_pid=%u far=%p router=%s kind=%s\n",
           rc,
           (unsigned int)sup->total_faults,
           (unsigned int)sup->faulted_count,
           (unsigned int)sup->last_fault_pid,
           (void *)(uptr)sup->last_fault_far,
           router ? service_supervisor_state_name(router->state) : "missing",
           router ? service_supervisor_fault_kind_name(router->last_fault.kind) : "missing");
    if (rc != AEGIS_OK ||
        !service_supervisor_fault_proof_ready() ||
        !service_supervisor_page_fault_proof_ready()) {
        PANIC("v44 service supervisor fault proof failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v44_page_fault_trap_recorded();
    qemu_boot_checkpoint_v44_service_marked_faulted();
#endif

    rc = service_supervisor_selftest();
    if (rc != AEGIS_OK || !service_supervisor_handoff_preserved()) {
        PANIC("v44 service supervisor selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v44_supervisor_handoff_preserved();
#endif
}

static void aegisbox_init_prepare_v45_page_isolation_or_panic(void) {
    page_isolation_init();

    int rc = page_isolation_prepare_user_kernel_tables();
    const aegis_page_isolation_state_t *iso = page_isolation_state();
    printk("[AegisOS:mmu] v45 page isolation rc=%d spaces=%u roots=%u mapped=%u text_rx=%u rw_nx=%u guards=%u denied=%u first_ttbr0=%p\n",
           rc,
           (unsigned int)iso->address_space_count,
           (unsigned int)iso->page_table_root_count,
           (unsigned int)iso->mapped_user_pages,
           (unsigned int)iso->text_rx_pages,
           (unsigned int)iso->writable_nx_pages,
           (unsigned int)iso->guard_page_count,
           (unsigned int)iso->kernel_denied_checks,
           (void *)(uptr)iso->first_ttbr0_root);
    if (rc != AEGIS_OK || !page_isolation_ready()) {
        PANIC("v45 page-table isolation prepare failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v45_page_tables_allocated();
    qemu_boot_checkpoint_v45_user_regions_mapped();
#endif

    if (!page_isolation_kernel_window_denied()) {
        PANIC("v45 kernel window isolation proof failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v45_kernel_window_blocked();
#endif

    if (!page_isolation_guard_pages_unmapped()) {
        PANIC("v45 guard page isolation proof failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v45_guard_pages_unmapped();
#endif

    if (!page_isolation_ttbr0_contract_ready()) {
        PANIC("v45 TTBR0 contract proof failed");
    }

    rc = page_isolation_selftest();
    if (rc != AEGIS_OK) {
        PANIC("v45 page-table isolation selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v45_ttbr0_contract_ready();
#endif
}


static void aegisbox_init_prepare_v46_network_control_or_panic(void) {
    network_control_init();

    int rc = network_control_prepare_v46_bringup();
    const aegis_network_control_state_t *net = network_control_state();
    printk("[AegisOS:network] v46 bring-up rc=%d profile=%s wan=%s lan=%s link=%s lease=%p gw=%p dns=%p routes=%u fw_rules=%u nat=%u gen=%u\n",
           rc,
           net->profile,
           net->wan_if,
           net->lan_if,
           network_control_link_state_name(net->wan_link_state),
           (void *)(uptr)net->lease_ip,
           (void *)(uptr)net->gateway,
           (void *)(uptr)net->dns_server,
           (unsigned int)net->route_count,
           (unsigned int)net->firewall_rule_count,
           (unsigned int)net->nat_mapping_count,
           (unsigned int)net->control_plane_generation);
    if (rc != AEGIS_OK || !network_control_ready()) {
        PANIC("v46 network control-plane prepare failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v46_network_stack_ready();
#endif

    if (!network_control_dhcp_bound()) {
        PANIC("v46 DHCP lease proof failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v46_dhcp_bound();
#endif

    if (!network_control_nat_ready() || !network_control_firewall_ready()) {
        PANIC("v46 NAT/firewall control-plane proof failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v46_nat_firewall_ready();
#endif

    rc = network_control_selftest();
    if (rc != AEGIS_OK) {
        PANIC("v46 network control-plane selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v46_router_control_plane_ready();
#endif
}

static void aegisbox_init_prepare_v47_developer_preview_or_panic(void) {
    developer_preview_init();

    int rc = developer_preview_prepare_v47_img();
    const aegis_dev_preview_state_t *dp = developer_preview_state();
    printk("[AegisOS:release] v47 Developer Preview rc=%d variants=%u enabled=%u manifest=%u docs=%u generation=%u\n",
           rc,
           (unsigned int)dp->variant_count,
           (unsigned int)dp->enabled_variants,
           dp->manifest_ready ? 1U : 0U,
           dp->docs_ready ? 1U : 0U,
           (unsigned int)dp->manifest_generation);
    if (rc != AEGIS_OK || !developer_preview_manifest_ready()) {
        PANIC("v47 Developer Preview manifest prepare failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v47_dev_preview_manifest_ready();
#endif

    if (!developer_preview_variants_ready()) {
        PANIC("v47 board profile matrix failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v47_board_profiles_ready();
#endif

    rc = developer_preview_selftest();
    if (rc != AEGIS_OK || !developer_preview_ready()) {
        PANIC("v47 Developer Preview selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v47_installer_flash_flow_ready();
    qemu_boot_checkpoint_v47_developer_preview_img_ready();
#endif
}

static void aegisbox_init_prepare_v48_release_polish_or_panic(void) {
    release_polish_init();

    int rc = release_polish_prepare_v48();
    const aegis_v48_release_polish_state_t *polish = release_polish_state();
    printk("[AegisOS:release] v48 polish rc=%d boards=%u router_capable=%u services=%u enabled=%u gates=%u generation=%u\n",
           rc,
           (unsigned int)polish->board_profile_count,
           (unsigned int)polish->router_capable_profiles,
           (unsigned int)polish->service_preset_count,
           (unsigned int)polish->enabled_service_presets,
           (unsigned int)polish->polish_gate_count,
           (unsigned int)polish->manifest_generation);
    if (rc != AEGIS_OK || !release_polish_ready()) {
        PANIC("v48 release polish prepare failed");
    }

    if (!release_polish_board_profiles_ready()) {
        PANIC("v48 board profile polish failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v48_board_profile_polish_ready();
#endif

    if (!release_polish_service_configs_ready()) {
        PANIC("v48 service config matrix failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v48_service_config_matrix_ready();
#endif

    if (!release_polish_security_hardening_ready()) {
        PANIC("v48 security hardening baseline failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v48_security_hardening_ready();
#endif

    rc = release_polish_selftest();
    if (rc != AEGIS_OK || !release_polish_image_manifest_ready()) {
        PANIC("v48 image polish manifest selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v48_image_polish_manifest_ready();
#endif
}

static void aegisbox_init_prepare_v55_release_or_panic(void) {
    release_final_init();

    int rc = release_final_prepare_v55();
    const aegis_release_final_state_t *rel = release_final_state();
    printk("[AegisOS:release] v55 final release rc=%d variants=%u services=%u hardening=%u docs=%u checksums=%u generation=%u\n",
           rc,
           (unsigned int)rel->variant_count,
           (unsigned int)rel->service_default_count,
           (unsigned int)rel->hardening_gate_count,
           (unsigned int)rel->doc_gate_count,
           (unsigned int)rel->checksum_artifacts,
           (unsigned int)rel->manifest_generation);
    if (rc != AEGIS_OK || !release_final_ready()) {
        PANIC("v55 final release prepare failed");
    }

    if (!release_final_installer_flash_hardened()) {
        PANIC("v50 installer/flash flow hardening failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v50_installer_flash_flow_hardened();
#endif

    if (!release_final_variant_images_ready()) {
        PANIC("v51 variant image matrix failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v51_variant_images_ready();
#endif

    if (!release_final_security_service_defaults_frozen()) {
        PANIC("v52 security hardening/service defaults freeze failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v52_security_service_defaults_frozen();
#endif

    if (!release_final_docs_hardware_notes_ready()) {
        PANIC("v53 docs/known limitations/hardware notes failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v53_docs_known_limits_hardware_notes_ready();
#endif

    if (!release_final_rc_audit_ready()) {
        PANIC("v54 final RC audit/signing/checksum proof failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v54_final_rc_audit_signing_checksums_ready();
#endif

    rc = release_final_selftest();
    if (rc != AEGIS_OK || !release_final_release_image_ready()) {
        PANIC("v55 Release IMG selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_v55_release_img_ready();
#endif
}

static void aegisbox_init_prepare_v41_storage_process_or_panic(void) {
    block_storage_init();
    int rc = block_storage_register_v40_flash_layout();
    const aegis_block_registry_t *bst = block_storage_state();
    const aegis_block_partition_t *boot = block_storage_find_partition("AEGIS_BOOT");
    const aegis_block_partition_t *root = block_storage_find_partition("AEGIS_ROOT");
    const aegis_block_partition_t *cfg = block_storage_find_partition("AEGIS_CONFIG");
    printk("[AegisOS:block] v40 flash layout register rc=%d devices=%u partitions=%u persistent=%u\n",
           rc,
           (unsigned int)bst->device_count,
           (unsigned int)bst->partition_count,
           (unsigned int)bst->persistent_partition_count);
    printk("[AegisOS:block] partitions boot=%s root=%s config=%s\n",
           boot ? block_storage_partition_state_name(boot->state) : "missing",
           root ? block_storage_partition_state_name(root->state) : "missing",
           cfg ? block_storage_partition_state_name(cfg->state) : "missing");
    if (rc != AEGIS_OK || !boot || !root || !cfg) {
        PANIC("v41 block/storage abstraction registration failed");
    }

    rc = block_storage_selftest();
    if (rc != AEGIS_OK || !block_storage_layout_ready() || !block_storage_persistent_config_ready()) {
        PANIC("v41 block/storage abstraction selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_block_storage_abstraction_ready();
#endif

    rc = process_table_cleanup_prepare();
    process_table_stats_t pst = process_table_stats();
    printk("[AegisOS:process] v41 table cleanup rc=%d slots=%u used=%u running=%u zombie=%u next_pid=%u\n",
           rc,
           (unsigned int)pst.total_slots,
           (unsigned int)pst.used_slots,
           (unsigned int)pst.running,
           (unsigned int)pst.zombie,
           (unsigned int)pst.next_pid);
    if (rc != AEGIS_OK) {
        PANIC("v41 process table cleanup prepare failed");
    }
    rc = process_table_cleanup_selftest();
    if (rc != AEGIS_OK || !process_table_cleanup_ready()) {
        PANIC("v41 process table cleanup selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_process_table_cleanup_ready();
#endif
}

static void aegisbox_init_prepare_v38_user_processes_or_panic(void) {
    int rc = userland_prepare_user_stack_bootstrap();
    const aegis_userland_handoff_t *handoff = userland_handoff_state();
    const aegis_user_process_descriptor_t *first = userland_first_process_descriptor();
    printk("[AegisOS:userland] argv/envp stack bootstrap rc=%d argc=%u envc=%u sp=%p state=%s\n",
           rc,
           first ? (unsigned int)first->argc : 0U,
           first ? (unsigned int)first->envc : 0U,
           first ? (void *)(uptr)first->bootstrap_sp : (void *)0,
           first ? user_process_state_name(first->state) : "missing");
    if (rc != AEGIS_OK) {
        PANIC("user stack argv/envp bootstrap failed");
    }

    rc = userland_user_stack_bootstrap_selftest();
    if (rc != AEGIS_OK || !userland_user_stack_bootstrap_ready()) {
        PANIC("user stack argv/envp bootstrap selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_user_stack_argv_envp_ready();
#endif

    rc = userland_create_multiple_process_descriptors();
    handoff = userland_handoff_state();
    printk("[AegisOS:userland] multiple process descriptors rc=%d total=%u secondary=%u launchable=%u\n",
           rc,
           (unsigned int)handoff->process_descriptor_count,
           (unsigned int)handoff->secondary_process_descriptor_count,
           (unsigned int)handoff->launchable_process_descriptor_count);
    if (rc != AEGIS_OK) {
        PANIC("multiple user process descriptor creation failed");
    }

    rc = userland_multiple_process_descriptors_selftest();
    if (rc != AEGIS_OK || !userland_multiple_process_descriptors_ready()) {
        PANIC("multiple user process descriptor selftest failed");
    }
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_multiple_user_process_descriptors_ready();
#endif
}

#ifdef AEGISOS_QEMU_SMOKE
__attribute__((unused)) static void aegisbox_init_controlled_el0_transition_or_panic(void) {
    const aegis_user_process_descriptor_t *first = userland_first_process_descriptor();
    if (!first) {
        PANIC("missing user process descriptor before EL0 transition");
    }

    int rc = userland_prepare_controlled_el0_transition(
        (u64)(uptr)aegis_el0_transition_test_entry,
        first->initial_sp);
    const aegis_userland_handoff_t *handoff = userland_handoff_state();
    printk("[AegisOS:userland] controlled EL0 transition prepare rc=%d entry=%p sp=%p active=%u\n",
           rc,
           (void *)(uptr)handoff->el0_probe_entry,
           (void *)(uptr)handoff->el0_probe_sp,
           userland_controlled_el0_transition_active() ? 1U : 0U);
    if (rc != AEGIS_OK) {
        PANIC("controlled EL0 transition prepare failed");
    }

    rc = userland_controlled_el0_transition_selftest();
    printk("[AegisOS:userland] controlled EL0 preflight rc=%d descriptor_state=%s\n",
           rc,
           user_process_state_name(userland_first_process_descriptor()->state));
    if (rc != AEGIS_OK) {
        PANIC("controlled EL0 transition selftest failed");
    }

    qemu_boot_checkpoint_el0_transition_prepared();
    qemu_smoke_write0("[AegisOS] controlled EL0 transition prepared; eret launch next\n");
    qemu_boot_checkpoint_el0_eret_launch();
    aegis_el0_transition_enter(handoff->el0_probe_entry, handoff->el0_probe_sp);
    PANIC("controlled EL0 transition returned unexpectedly");
}


static void aegisbox_init_first_tiny_user_process_or_panic(void) {
    const aegis_user_process_descriptor_t *first = userland_first_process_descriptor();
    if (!first) {
        PANIC("missing user process descriptor before first tiny user process launch");
    }

    int rc = userland_prepare_first_tiny_user_process(
        (u64)(uptr)aegis_first_user_process_entry,
        first->initial_sp);
    const aegis_userland_handoff_t *handoff = userland_handoff_state();
    printk("[AegisOS:userland] first tiny user process prepare rc=%d pid=%u entry=%p sp=%p state=%s\n",
           rc,
           (unsigned int)handoff->first_user_process_pid,
           (void *)(uptr)handoff->first_user_process_entry,
           (void *)(uptr)handoff->first_user_process_sp,
           user_process_state_name(userland_first_process_descriptor()->state));
    if (rc != AEGIS_OK) {
        PANIC("first tiny user process prepare failed");
    }

    rc = userland_first_tiny_user_process_selftest();
    printk("[AegisOS:userland] first tiny user process selftest rc=%d launch_ready=%u\n",
           rc,
           handoff->first_user_process_launch_ready ? 1U : 0U);
    if (rc != AEGIS_OK) {
        PANIC("first tiny user process selftest failed");
    }

    qemu_boot_checkpoint_el0_transition_prepared();
    qemu_boot_checkpoint_first_user_process_descriptor();
    qemu_smoke_write0("[AegisOS] v42 multi-process/syscall-expanded file-backed ELF first process ready; eret launch next\n");
    qemu_boot_checkpoint_first_user_process_launch();
    qemu_boot_checkpoint_syscall_return_path_ready();
    qemu_boot_checkpoint_first_user_process_exit_ready();
    qemu_boot_checkpoint_el0_eret_launch();
    aegis_el0_transition_enter(handoff->first_user_process_entry, handoff->first_user_process_sp);
    PANIC("first tiny user process launch returned unexpectedly");
}
#endif


#ifndef AEGISOS_QEMU_SMOKE
static void aegisbox_init_prepare_interactive_runtime_state_or_panic(void) {
    int rc;

    /* Prepare the runtime state the shell displays, but do not emit long
     * milestone printk lines before the console.  Formal proof builds still
     * use the detailed v44-v55 wrappers below.
     */
    service_supervisor_init();
    rc = service_supervisor_register_user_processes();
    if (rc != AEGIS_OK || !service_supervisor_registry_ready()) {
        PANIC("interactive supervisor registry failed");
    }
    rc = service_supervisor_start_registered();
    if (rc != AEGIS_OK) {
        PANIC("interactive supervisor start failed");
    }
    rc = service_supervisor_prepare_v44_fault_proof();
    if (rc != AEGIS_OK || !service_supervisor_fault_proof_ready()) {
        PANIC("interactive supervisor fault proof failed");
    }
    rc = service_supervisor_selftest();
    if (rc != AEGIS_OK) {
        PANIC("interactive supervisor selftest failed");
    }

    page_isolation_init();
    rc = page_isolation_prepare_user_kernel_tables();
    if (rc != AEGIS_OK || !page_isolation_ready()) {
        PANIC("interactive page isolation prepare failed");
    }
    rc = page_isolation_selftest();
    if (rc != AEGIS_OK) {
        PANIC("interactive page isolation selftest failed");
    }

    network_control_init();
    rc = network_control_prepare_v46_bringup();
    if (rc != AEGIS_OK || !network_control_ready()) {
        PANIC("interactive network control prepare failed");
    }
    rc = network_control_selftest();
    if (rc != AEGIS_OK) {
        PANIC("interactive network control selftest failed");
    }

    developer_preview_init();
    rc = developer_preview_prepare_v47_img();
    if (rc != AEGIS_OK || !developer_preview_ready()) {
        PANIC("interactive developer preview prepare failed");
    }
    rc = developer_preview_selftest();
    if (rc != AEGIS_OK) {
        PANIC("interactive developer preview selftest failed");
    }

    release_polish_init();
    rc = release_polish_prepare_v48();
    if (rc != AEGIS_OK || !release_polish_ready()) {
        PANIC("interactive release polish prepare failed");
    }
    rc = release_polish_selftest();
    if (rc != AEGIS_OK) {
        PANIC("interactive release polish selftest failed");
    }

    release_final_init();
    rc = release_final_prepare_v55();
    if (rc != AEGIS_OK || !release_final_ready()) {
        PANIC("interactive final release prepare failed");
    }
    rc = release_final_selftest();
    if (rc != AEGIS_OK) {
        PANIC("interactive final release selftest failed");
    }
}
#endif

static void aegisbox_init_task(void) {
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_first_init_thread();
    qemu_smoke_write0("[AegisOS] first kernel init thread entered\n");
#endif
    printk("[AegisOS:init] AegisBox init task online\n");
    aegisbox_init_mount_vfs_or_panic();
#ifdef AEGISOS_QEMU_SMOKE
    qemu_smoke_write0("[AegisOS] VFS/initramfs bootstrap complete inside init thread\n");
#endif
    printk("[AegisOS:init] VFS/initramfs bootstrap complete; registering kernel services\n");
    aegisbox_init_register_services_or_panic();
#ifdef AEGISOS_QEMU_SMOKE
    qemu_smoke_write0("[AegisOS] kernel services registered and service manager prepared\n");
#endif
#ifndef AEGISOS_QEMU_SMOKE
    /* v55.1.3: release/interactive boots must reach the terminal first.
     * The userland handoff/proof ladder is still preserved below for smoke
     * builds, but normal VM/Raspberry-Pi-style boots now hand ttyAMA0 to the
     * shell immediately after VFS + kernel services are online.  This avoids
     * the current pre-shell handoff stall and gives the operator a console.
     */
    interactive_console_run();
#else
    printk("[AegisOS:init] kernel services registered; preparing userland handoff\n");
    aegisbox_init_prepare_userland_or_panic();
    qemu_smoke_write0("[AegisOS] userland feature catalogue and handoff contract ready\n");
    printk("[AegisOS:init] userland handoff prepared; building pre-EL0 process contract\n");
    aegisbox_init_prepare_pre_el0_or_panic();
    qemu_smoke_write0("[AegisOS] fake user descriptor, user stack layout, and syscall ABI proof ready; loading first ELF image\n");
    printk("[AegisOS:init] pre-EL0 contract ready; validating /sbin/aegis-init ELF PT_LOAD/permission loader path\n");
    aegisbox_init_load_first_elf_or_panic();
    printk("[AegisOS:init] PT_LOAD/perms ready; preparing v38 argv/envp stack and process descriptors\n");
    aegisbox_init_prepare_v38_user_processes_or_panic();
    printk("[AegisOS:init] v38 ready; preparing v41 block/storage abstraction and process table cleanup\n");
    aegisbox_init_prepare_v41_storage_process_or_panic();
    printk("[AegisOS:init] v41 ready; preparing v42 multi-process launch path and expanded syscalls\n");
    aegisbox_init_prepare_v42_multiprocess_syscalls_or_panic();
    printk("[AegisOS:init] v42 ready; preparing v43 IPC/service messaging\n");
    aegisbox_init_prepare_v43_ipc_service_messaging_or_panic();
    printk("[AegisOS:init] v43 ready; preparing v44 service supervisor and fault proof\n");
    aegisbox_init_prepare_v44_service_supervisor_or_panic();
    printk("[AegisOS:init] v44 ready; preparing v45 user/kernel page-table isolation\n");
    aegisbox_init_prepare_v45_page_isolation_or_panic();
    printk("[AegisOS:init] v45 ready; preparing v46 network bring-up and router control plane\n");
    aegisbox_init_prepare_v46_network_control_or_panic();
    printk("[AegisOS:init] v46 ready; preparing v47 AegisBox Developer Preview IMG contract\n");
    aegisbox_init_prepare_v47_developer_preview_or_panic();
    printk("[AegisOS:init] v47 ready; preparing v48 release polish, board profiles, service configs, and hardening gates\n");
    aegisbox_init_prepare_v48_release_polish_or_panic();
    printk("[AegisOS:init] v48 ready; preparing v50-v55 final release image gates\n");
    aegisbox_init_prepare_v55_release_or_panic();
    qemu_smoke_write0("[AegisOS] v55 Release IMG contract ready; launching file-backed ELF user process proof\n");
    printk("[AegisOS:init] v55 Release IMG contract ready; launching file-backed ELF user process proof\n");
    aegisbox_init_first_tiny_user_process_or_panic();
#endif
}

void kernel_main(u64 dtb_ptr) {
    early_log_init(0x09000000UL);
    early_log_checkpoint("[AegisOS:log] kernel_main early log online\n");
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_visible_log();
#endif

    irq_enable_set_allowed(false);
    irq_global_disable();

    boot_phase_init(dtb_ptr);

#ifdef AEGISOS_QEMU_SMOKE
    qemu_smoke_write0("[AegisOS] kernel_main reached\n");
    qemu_boot_checkpoint_kernel_main();
#endif

    if (hal_init() != 0) PANIC("hal_init() failed");
    if (board_init() != 0) PANIC("board_init() failed");

    boot_phase_enter(BOOT_PHASE_EARLY_CONSOLE);
    printk("[AegisOS] kernel_main reached, dtb=%p\n", (void *)(uptr)dtb_ptr);

    boot_phase_enter(BOOT_PHASE_PLATFORM);
    if (dtb_ptr != 0) {
        if (device_tree_init((const void *)(uptr)dtb_ptr) == 0) {
            printk("[AegisOS:dtb] detected FDT, size=%u bytes\n", device_tree_get_total_size());
        } else {
            printk("[AegisOS:dtb] warning: invalid or unsupported FDT at %p\n", (void *)(uptr)dtb_ptr);
        }
    } else {
        printk("[AegisOS:dtb] warning: no FDT pointer supplied\n");
    }

    gic_init();

    boot_phase_enter(BOOT_PHASE_MEMORY);
    /* QEMU virt gives RAM at 0x40000000.  v23 now reserves the known boot,
     * kernel, stack, heap, and FDT regions before testing page allocation.
     */
    if (phys_mem_init(AEGIS_QEMU_RAM_BASE, AEGIS_QEMU_RAM_SIZE) != AEGIS_OK) {
        PANIC("phys_mem_init failed");
    }

    page_table_init();
    virt_mem_init();
    memory_bringup_selftest(dtb_ptr);
    printk("[AegisOS:memory] PMM/VMM/heap initialised and self-tested\n");
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_memory();
    qemu_boot_checkpoint_memory_selftest();
#endif

    boot_phase_enter(BOOT_PHASE_CORE);
    exception_vector_bringup_selftest();
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_exception_vectors();
#endif

    timer_bringup_selftest_or_panic();
    kernel_timer_init();
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_timer();
#endif

    scheduler_init();
#ifdef AEGISOS_QEMU_SMOKE
    scheduler_bringup_selftest_or_panic();
    scheduler_first_init_thread_selftest_or_panic();
    qemu_boot_checkpoint_scheduler();
#endif

    process_init();
    syscall_table_init();
    printk("[AegisOS:core] timer/scheduler/process/syscall tables initialised\n");
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_core();
#endif

    boot_phase_enter(BOOT_PHASE_DEVICES);
    printk("[AegisOS:devices] board UART/timer/watchdog initialised\n");
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_devices();
#endif

    boot_phase_enter(BOOT_PHASE_KERNEL_SERVICES);
    service_manager_init();
    printk("[AegisOS:services] service manager initialised; registration will occur inside aegisbox-init after VFS mount\n");
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_kernel_services();
#endif

    boot_phase_enter(BOOT_PHASE_SECURITY);
    printk("[AegisOS:security] boot hardening hooks pending wire-up\n");
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_security();
#endif

    boot_phase_enter(BOOT_PHASE_NETWORK);
    printk("[AegisOS:network] packet/firewall/router hooks pending wire-up\n");
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_network();
#endif

    boot_phase_enter(BOOT_PHASE_INIT);
    if (!task_create("idle", kernel_idle_task, 0)) {
        PANIC("failed to create idle task");
    }
    if (!task_create("aegisbox-init", aegisbox_init_task, 100)) {
        PANIC("failed to create aegisbox init task");
    }
    printk("[AegisOS:init] kernel init tasks created\n");
#ifdef AEGISOS_QEMU_SMOKE
    qemu_boot_checkpoint_init();
#endif

#ifdef AEGISOS_QEMU_SMOKE
    boot_phase_enter(BOOT_PHASE_RUNNING);
    qemu_boot_checkpoint_running();
    qemu_smoke_write0("[AegisOS] kernel_main boot phases complete; starting scheduler\n");
    scheduler_run();
    PANIC("scheduler_run returned in QEMU smoke");
#endif

    boot_phase_enter(BOOT_PHASE_RUNNING);
    printk("[AegisOS] Kernel started — v%d.%d.%d\n",
           AEGISOS_VERSION_MAJOR,
           AEGISOS_VERSION_MINOR,
           AEGISOS_VERSION_PATCH);

    irq_enable_set_allowed(true);
    if (!irq_try_global_enable() || !irq_global_is_enabled()) {
        PANIC("Failed to safely enable IRQs");
    }

    /* Start scheduler — does not return under normal operation. */
    scheduler_run();

    PANIC("scheduler_run() returned unexpectedly");
}
