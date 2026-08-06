/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/syscall.c
 * System call implementations.
 */

#include "types.h"
#include "memory.h"
#include "scheduler.h"
#include "syscalls.h"
#include "userland.h"
#include "process.h"
#include "panic.h"
#include "elf_loader.h"
#include "ipc_channel.h"
#include "service_supervisor.h"
#include "interactive_console.h"
#include "kernel_timer.h"
#include "power.h"
#include "socket.h"
#include "entropy.h"
#include "rtc.h"
#include "fd_table.h"
#include "vfs.h"

#define AEGIS_SYSCALL_PATH_MAX     128UL
#define AEGIS_SYSCALL_MSG_MAX      256UL
#define AEGIS_SYSCALL_IO_CHUNK     128UL

/* Forward declarations from other modules */
extern u32 process_getpid(void);
extern void process_exit(int code);
extern void printk(const char *fmt, ...);

/* IPC/capability forward declarations */
extern int ipc_send(u32 channel_id, const void *data, u64 len);
extern int ipc_recv(u32 channel_id, void *buf, u64 max_len);
extern bool cap_is_valid(cap_token_t tok);
extern s64 cap_revoke(cap_token_t tok);

static bool range_end(uptr ptr, u64 len, uptr *end_out) {
    if (ptr == 0 || len == 0) return false;
    uptr end = ptr + (uptr)len;
    if (end <= ptr) return false;
    if (end_out) *end_out = end;
    return true;
}

static bool range_inside(uptr ptr, u64 len, uptr base, u64 size) {
    uptr end = 0;
    if (!range_end(ptr, len, &end)) return false;
    uptr region_end = base + (uptr)size;
    if (region_end <= base) return false;
    return ptr >= base && end <= region_end;
}

static bool user_read_range_ok(u64 ptr, u64 len) {
    if (process_getpid() != 0) return process_current_user_read_range_ok(ptr, len);
    uptr p = (uptr)ptr;
    return range_inside(p, len, AEGIS_USER_TEXT_BASE,  AEGIS_USER_TEXT_SIZE)  ||
           range_inside(p, len, AEGIS_USER_HEAP_BASE,  AEGIS_USER_HEAP_SIZE)  ||
           range_inside(p, len, AEGIS_USER_IPC_BASE,   AEGIS_USER_IPC_SIZE)   ||
           range_inside(p, len, AEGIS_USER_STACK_TOP - AEGIS_USER_STACK_SIZE,
                        AEGIS_USER_STACK_SIZE) ||
           elf_loader_runtime_read_range_ok(ptr, len);
}

static bool user_write_range_ok(u64 ptr, u64 len) {
    if (process_getpid() != 0) return process_current_user_write_range_ok(ptr, len);
    uptr p = (uptr)ptr;
    return range_inside(p, len, AEGIS_USER_HEAP_BASE,  AEGIS_USER_HEAP_SIZE)  ||
           range_inside(p, len, AEGIS_USER_IPC_BASE,   AEGIS_USER_IPC_SIZE)   ||
           range_inside(p, len, AEGIS_USER_STACK_TOP - AEGIS_USER_STACK_SIZE,
                        AEGIS_USER_STACK_SIZE) ||
           elf_loader_runtime_write_range_ok(ptr, len);
}

static int copy_from_user_checked(void *dst, u64 user_src, u64 len) {
    if (!dst || len == 0) return AEGIS_EINVAL;
    if (!user_read_range_ok(user_src, len)) return AEGIS_EINVAL;
    const volatile u8 *src = (const volatile u8 *)(uptr)user_src;
    u8 *out = (u8 *)dst;
    for (u64 i = 0; i < len; i++) out[i] = src[i];
    return AEGIS_OK;
}

static int copy_to_user_checked(u64 user_dst, const void *src, u64 len) {
    if (!src || len == 0) return AEGIS_EINVAL;
    if (!user_write_range_ok(user_dst, len)) return AEGIS_EINVAL;
    volatile u8 *dst = (volatile u8 *)(uptr)user_dst;
    const u8 *in = (const u8 *)src;
    for (u64 i = 0; i < len; i++) dst[i] = in[i];
    return AEGIS_OK;
}

static int copy_string_from_user(char *dst, u64 dst_len, u64 user_src) {
    if (!dst || dst_len == 0 || !user_src) return AEGIS_EINVAL;
    for (u64 i = 0; i + 1 < dst_len; i++) {
        if (!user_read_range_ok(user_src + i, 1)) return AEGIS_EINVAL;
        char c = *(const volatile char *)(uptr)(user_src + i);
        dst[i] = c;
        if (c == '\0') return AEGIS_OK;
    }
    dst[dst_len - 1U] = '\0';
    return AEGIS_EINVAL;
}


static bool kernel_str_eq(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static bool current_process_may_manage_cap(cap_token_t cap) {
    u32 current_pid = process_getpid();
    if (current_pid == 0) return true; /* kernel/bootstrap context */
    return process_has_cap(current_pid, cap);
}

/* ---- Syscall implementations ---- */

s64 sys_read(u64 fd, u64 buf, u64 len, u64 a3, u64 a4, u64 a5) {
    (void)a3; (void)a4; (void)a5;
    if (!buf || len == 0 || !user_write_range_ok(buf, len)) return AEGIS_EINVAL;
    if (fd == 0) {
        extern int uart_getchar(void);
        u64 count = 0;
        for (u64 i = 0; i < len; i++) {
            int ch = uart_getchar();
            if (ch < 0) break;
            u8 c = (u8)ch;
            int rc = copy_to_user_checked(buf + i, &c, 1);
            if (rc != AEGIS_OK) return rc;
            count++;
        }
        return (s64)count;
    }
    u32 pid = process_getpid();
    if (fd >= AEGIS_SOCKET_FD_BASE) {
        u8 tmp[AEGIS_SYSCALL_IO_CHUNK];
        u64 done = 0;
        while (done < len) {
            u32 chunk = (u32)(((len - done) < sizeof(tmp)) ? (len - done) : sizeof(tmp));
            int got = net_socket_recv(pid, (int)fd, tmp, chunk);
            if (got == AEGIS_EAGAIN && done != 0) break;
            if (got < 0) return done ? (s64)done : (s64)got;
            if (got == 0) break;
            int rc = copy_to_user_checked(buf + done, tmp, (u64)got);
            if (rc != AEGIS_OK) return rc;
            done += (u64)got;
            if ((u32)got < chunk) break;
        }
        return (s64)done;
    }
    u64 *offset = NULL;
    void *vn = fd_get(pid, (int)fd, false, &offset);
    if (!vn || !offset) return AEGIS_ENOENT;
    u8 tmp[AEGIS_SYSCALL_IO_CHUNK];
    u64 done = 0;
    while (done < len) {
        u64 chunk = (len - done) < sizeof(tmp) ? (len - done) : sizeof(tmp);
        int got = vfs_read(vn, *offset, tmp, chunk);
        if (got < 0) return done ? (s64)done : (s64)got;
        if (got == 0) break;
        int rc = copy_to_user_checked(buf + done, tmp, (u64)got);
        if (rc != AEGIS_OK) return rc;
        *offset += (u64)got;
        done += (u64)got;
        if ((u64)got < chunk) break;
    }
    return (s64)done;
}

s64 sys_write(u64 fd, u64 buf, u64 len, u64 a3, u64 a4, u64 a5) {
    (void)a3; (void)a4; (void)a5;
    if (!buf || len == 0 || !user_read_range_ok(buf, len)) return AEGIS_EINVAL;
    if (fd == 1 || fd == 2) {
        extern void uart_putchar(char c);
        u8 tmp[AEGIS_SYSCALL_IO_CHUNK];
        u64 off = 0;
        while (off < len) {
            u64 chunk = (len - off) < AEGIS_SYSCALL_IO_CHUNK ? (len - off) : AEGIS_SYSCALL_IO_CHUNK;
            int rc = copy_from_user_checked(tmp, buf + off, chunk);
            if (rc != AEGIS_OK) return rc;
            for (u64 i = 0; i < chunk; i++) uart_putchar((char)tmp[i]);
            off += chunk;
        }
        return (s64)len;
    }
    u32 pid = process_getpid();
    if (fd >= AEGIS_SOCKET_FD_BASE) {
        u8 tmp[AEGIS_SYSCALL_IO_CHUNK];
        u64 done = 0;
        while (done < len) {
            u32 chunk = (u32)(((len - done) < sizeof(tmp)) ? (len - done) : sizeof(tmp));
            int rc = copy_from_user_checked(tmp, buf + done, chunk);
            if (rc != AEGIS_OK) return rc;
            int sent = net_socket_send(pid, (int)fd, tmp, chunk);
            if (sent < 0) return done ? (s64)done : (s64)sent;
            done += (u64)sent;
            if ((u32)sent < chunk) break;
        }
        return (s64)done;
    }
    u64 *offset = NULL;
    void *vn = fd_get(pid, (int)fd, true, &offset);
    if (!vn || !offset) return AEGIS_ENOENT;
    u8 tmp[AEGIS_SYSCALL_IO_CHUNK];
    u64 done = 0;
    while (done < len) {
        u64 chunk = (len - done) < sizeof(tmp) ? (len - done) : sizeof(tmp);
        int rc = copy_from_user_checked(tmp, buf + done, chunk);
        if (rc != AEGIS_OK) return rc;
        int wrote = vfs_write(vn, *offset, tmp, chunk);
        if (wrote < 0) return done ? (s64)done : (s64)wrote;
        *offset += (u64)wrote;
        done += (u64)wrote;
        if ((u64)wrote < chunk) break;
    }
    return (s64)done;
}

s64 sys_open(u64 path_ptr, u64 flags, u64 mode, u64 a3, u64 a4, u64 a5) {
    (void)flags; (void)mode; (void)a3; (void)a4; (void)a5;
    char path[AEGIS_SYSCALL_PATH_MAX];
    int rc = copy_string_from_user(path, sizeof(path), path_ptr);
    if (rc != AEGIS_OK) return rc;
    vnode_t *vn = vfs_open(path);
    if (!vn) return AEGIS_ENOENT;
    int fd = fd_install(process_getpid(), vn, true, true);
    if (fd < 0) vfs_close(vn);
    return fd;
}

s64 sys_close(u64 fd, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    if (fd <= 2) return AEGIS_OK;
    u32 pid = process_getpid();
    if (fd >= AEGIS_SOCKET_FD_BASE) return net_socket_close(pid, (int)fd);
    return fd_close(pid, (int)fd);
}

s64 sys_exit(u64 code, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    process_exit((int)code);
    return 0;
}

s64 sys_getpid(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return (s64)process_getpid();
}

s64 sys_mmap(u64 addr, u64 length, u64 prot, u64 flags, u64 fd, u64 offset) {
    (void)flags; (void)fd; (void)offset;
    if (length == 0) return AEGIS_EINVAL;
    bool want_write = (prot & 0x2U) != 0;
    bool want_exec  = (prot & 0x4U) != 0;
    if (want_write && want_exec) return AEGIS_EPERM;

    u64 pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    u64 map_len = pages * PAGE_SIZE;
    virt_addr_t va = addr ? (virt_addr_t)addr : (AEGIS_USER_HEAP_BASE + (AEGIS_USER_HEAP_SIZE / 2U));
    if ((va & (PAGE_SIZE - 1U)) != 0) return AEGIS_EINVAL;
    if (!user_write_range_ok(va, map_len)) return AEGIS_EINVAL;

    u32 vm_flags = AEGIS_VM_USER | AEGIS_VM_READ;
    if (want_write || prot == 0) vm_flags |= AEGIS_VM_WRITE;
    if (want_exec) vm_flags |= AEGIS_VM_EXEC;

    for (u64 i = 0; i < pages; i++) {
        phys_addr_t pa = phys_alloc_page();
        if (pa == 0) return AEGIS_ENOMEM;
        int rc = virt_map(va + i * PAGE_SIZE, pa, PAGE_SIZE, vm_flags);
        if (rc != AEGIS_OK) return rc;
    }
    return (s64)va;
}

s64 sys_munmap(u64 addr, u64 length, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    if (!addr || length == 0 || !user_write_range_ok(addr, length)) return AEGIS_EINVAL;
    virt_unmap(addr, length);
    return AEGIS_OK;
}

s64 sys_send_msg(u64 channel_id, u64 data_ptr, u64 data_len, u64 a3, u64 a4, u64 a5) {
    (void)a3; (void)a4; (void)a5;
    if (!data_ptr || data_len == 0 || data_len > AEGIS_SYSCALL_MSG_MAX) return AEGIS_EINVAL;
    if (!user_read_range_ok(data_ptr, data_len)) return AEGIS_EINVAL;
    channel_t *ch = channel_get((u32)channel_id);
    if (!ch) return AEGIS_ENOENT;
    u8 msg[AEGIS_SYSCALL_MSG_MAX];
    int rc = copy_from_user_checked(msg, data_ptr, data_len);
    if (rc != AEGIS_OK) return rc;
    return (s64)ipc_send((u32)channel_id, msg, data_len);
}

s64 sys_recv_msg(u64 channel_id, u64 buf_ptr, u64 buf_len, u64 a3, u64 a4, u64 a5) {
    (void)a3; (void)a4; (void)a5;
    if (!buf_ptr || buf_len == 0 || buf_len > AEGIS_SYSCALL_MSG_MAX) return AEGIS_EINVAL;
    if (!user_write_range_ok(buf_ptr, buf_len)) return AEGIS_EINVAL;
    channel_t *ch = channel_get((u32)channel_id);
    if (!ch) return AEGIS_ENOENT;
    u8 msg[AEGIS_SYSCALL_MSG_MAX];
    int got = ipc_recv((u32)channel_id, msg, buf_len);
    if (got < 0) return (s64)got;
    int rc = copy_to_user_checked(buf_ptr, msg, (u64)got);
    return rc == AEGIS_OK ? (s64)got : (s64)rc;
}

s64 sys_cap_grant(u64 target_pid, u64 cap_token, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    if (target_pid == 0 || cap_token == NULL_CAP) return AEGIS_EINVAL;
    if (!cap_is_valid((cap_token_t)cap_token)) return AEGIS_EINVAL;
    if (!current_process_may_manage_cap((cap_token_t)cap_token)) return AEGIS_EPERM;
    return (s64)process_grant_cap((u32)target_pid, (cap_token_t)cap_token);
}

s64 sys_cap_revoke(u64 target_pid, u64 cap_token, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    if (target_pid == 0 || cap_token == NULL_CAP) return AEGIS_EINVAL;
    if (!cap_is_valid((cap_token_t)cap_token)) return AEGIS_EINVAL;
    if (!current_process_may_manage_cap((cap_token_t)cap_token)) return AEGIS_EPERM;
    int ret = process_revoke_cap((u32)target_pid, (cap_token_t)cap_token);
    if (ret != AEGIS_OK) return ret;
    return cap_revoke((cap_token_t)cap_token);
}

s64 sys_yield(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    scheduler_yield();
    return AEGIS_OK;
}

s64 sys_spawn(u64 path_ptr, u64 flags, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)flags; (void)a2; (void)a3; (void)a4; (void)a5;
    char path[AEGIS_SYSCALL_PATH_MAX];
    int rc = copy_string_from_user(path, sizeof(path), path_ptr);
    if (rc != AEGIS_OK) return rc;

    const aegis_userland_feature_t *feature = userland_find_feature_by_path(path);
    if (!feature || (feature->flags & AEGIS_USERLAND_FLAG_PROCESS) == 0) return AEGIS_ENOENT;
    if (feature->live_pid != 0) return AEGIS_EBUSY;

    const bool is_control_plane =
        kernel_str_eq(feature->name, "service-manager") ||
        kernel_str_eq(feature->name, "aegisd");
    const bool supervise = feature->service_id != 0U;
    bool supervised_restart = false;
    if (supervise) {
        supervised_restart = service_supervisor_find(feature->name) != NULL;
        u64 retry_after = 0;
        if (supervised_restart) {
            int allowed = service_supervisor_restart_allowed(feature->name,
                                                             service_supervisor_clock_ticks(),
                                                             &retry_after);
            if (allowed != AEGIS_OK) return allowed;
        }
    }

    aegis_elf_image_info_t image;
    rc = elf_loader_load_vfs_path(path, &image);
    if (rc != AEGIS_OK) return rc;

    u32 ppid = process_getpid();
    process_t *child = process_spawn_user_image(feature->name,
                                                image.runtime_entry,
                                                image.text_kernel_backing,
                                                image.text_memsz,
                                                image.runtime_stack_base,
                                                image.runtime_stack_size,
                                                image.runtime_slot,
                                                ppid);
    if (!child) {
        (void)elf_loader_release_runtime_slot(image.runtime_slot);
        return AEGIS_ENOMEM;
    }
    u32 pid = process_pid(child);

    rc = userland_note_native_process_spawn(feature->name,
                                            path,
                                            pid,
                                            image.runtime_entry,
                                            image.text_kernel_backing,
                                            image.text_memsz,
                                            image.runtime_stack_base,
                                            image.runtime_stack_size);
    if (rc != AEGIS_OK) {
        (void)process_kill(pid, 9);
        return rc;
    }

    if (supervise) {
        u32 policy = AEGIS_SUPERVISOR_POLICY_RESTART_ON_FAULT;
        if (is_control_plane) {
            policy |= AEGIS_SUPERVISOR_POLICY_REQUIRED |
                      AEGIS_SUPERVISOR_POLICY_CONTROL_PLANE;
        }
        rc = service_supervisor_native_register_starting(feature->name,
                                                         feature->service_id,
                                                         pid,
                                                         policy);
        if (rc != AEGIS_OK) {
            (void)process_kill(pid, 9);
            return rc;
        }
        if (supervised_restart) {
            rc = service_supervisor_note_restart(feature->name, pid, service_supervisor_clock_ticks());
            if (rc != AEGIS_OK) return rc;
        }
    }

    return (s64)pid;
}

s64 sys_waitpid(u64 pid, u64 exit_code_ptr, u64 options, u64 a3, u64 a4, u64 a5) {
    (void)a3; (void)a4; (void)a5;
    int code = 0;
    u32 reaped_pid = (u32)pid;
    int rc;
    if (pid == (u64)-1 || pid == 0xffffffffULL) {
        rc = process_wait_any(process_getpid(), &reaped_pid, &code);
    } else {
        if (pid == 0) return AEGIS_EINVAL;
        rc = process_wait((u32)pid, &code);
        if (rc == AEGIS_EBUSY) rc = AEGIS_EAGAIN;
    }
    if (rc == AEGIS_EAGAIN && (options & AEGIS_WAIT_NOHANG) != 0U) return 0;
    if (rc == AEGIS_OK && exit_code_ptr) {
        int cr = copy_to_user_checked(exit_code_ptr, &code, sizeof(code));
        if (cr != AEGIS_OK) return cr;
    }
    return rc == AEGIS_OK ? (s64)reaped_pid : (s64)rc;
}

s64 sys_getppid(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return (s64)process_getppid();
}

s64 sys_gettid(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    task_t *cur = scheduler_current();
    return cur ? (s64)cur->tid : 0;
}

s64 sys_service_id(u64 name_ptr, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    char name[AEGIS_USERLAND_NAME_MAX];
    int rc = copy_string_from_user(name, sizeof(name), name_ptr);
    if (rc != AEGIS_OK) return rc;
    u32 id = userland_feature_id_by_name(name);
    return id ? (s64)id : AEGIS_ENOENT;
}


s64 sys_channel_open(u64 name_ptr, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    char name[AEGIS_NAMED_CHANNEL_NAME_MAX];
    int rc = copy_string_from_user(name, sizeof(name), name_ptr);
    if (rc != AEGIS_OK) return rc;
    u32 owner = process_getpid();
    if (owner == 0) return AEGIS_EPERM;
    return (s64)channel_open_named(name, owner);
}

s64 sys_service_ready(u64 name_ptr, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    char name[AEGIS_USERLAND_NAME_MAX];
    int rc = copy_string_from_user(name, sizeof(name), name_ptr);
    if (rc != AEGIS_OK) return rc;

    u32 pid = process_getpid();
    const char *current_name = process_current_name();
    if (!current_name || pid == 0) return AEGIS_EPERM;
    u32 i = 0;
    while (name[i] && current_name[i] && name[i] == current_name[i]) i++;
    if (name[i] != '\0' || current_name[i] != '\0') return AEGIS_EPERM;

    rc = userland_mark_feature_native_running(name, pid);
    if (rc != AEGIS_OK) return rc;

    if (pid != 1U) {
        rc = service_supervisor_native_mark_running(name, pid);
        if (rc != AEGIS_OK && rc != AEGIS_ENOENT) return rc;
        if (rc == AEGIS_OK) {
            printk("[AegisOS:supervisor] %s pid=%u state=native-running\n", name, pid);
        } else {
            printk("[AegisOS:userland] %s pid=%u state=native-running\n", name, pid);
        }
    } else {
        printk("[AegisOS:userland] aegis-init pid=1 state=native-running\n");
    }
    return AEGIS_OK;
}


s64 sys_console_ready(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;

    /* Only the native service-manager may release the foreground shell, and
     * only after both control-plane processes are genuinely running.  The
     * service-manager invokes this syscall after it has validated aegisd's
     * fixed IPC health payload, so this is the guest-side boot-complete gate.
     */
    const char *current_name = process_current_name();
    if (!current_name || !kernel_str_eq(current_name, "service-manager")) {
        return AEGIS_EPERM;
    }

    const aegis_supervised_service_t *manager =
        service_supervisor_find("service-manager");
    const aegis_supervised_service_t *daemon =
        service_supervisor_find("aegisd");
    if (!manager || !daemon ||
        manager->state != AEGIS_SUPERVISED_RUNNING ||
        daemon->state != AEGIS_SUPERVISED_RUNNING) {
        return AEGIS_EBUSY;
    }

    if (!interactive_console_is_released()) {
        interactive_console_release();
        printk("[AegisOS:console] supervisor recovery health confirmed; releasing ttyAMA0 shell\n");
    }
    return AEGIS_OK;
}

s64 sys_kill(u64 pid, u64 signal, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    if (!pid || signal == 0 || signal > 64U) return AEGIS_EINVAL;
    u32 caller = process_getpid();
    process_t *target = process_get((u32)pid);
    if (!target) return AEGIS_ENOENT;
    /* PID 1 may administer all userspace.  Ordinary processes may signal
     * themselves or direct children only.  service-manager is the explicit
     * policy exception for supervised services.  Do not compare the caller's
     * PPID to the target PID: that incorrectly authorises siblings/parents and
     * rejects a caller's real child.
     */
    if (caller != 1U &&
        caller != (u32)pid &&
        process_parent_pid(target) != caller) {
        const char *name = process_current_name();
        if (!name || !kernel_str_eq(name, "service-manager")) return AEGIS_EPERM;
    }
    return process_kill((u32)pid, (int)signal);
}

s64 sys_sleep(u64 milliseconds, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    if (milliseconds > 86400000ULL) return AEGIS_EINVAL;
    u64 deadline = monotonic_nanoseconds() + milliseconds * 1000000ULL;
    while (monotonic_nanoseconds() < deadline) scheduler_yield();
    return AEGIS_OK;
}

typedef struct aegis_timespec {
    u64 seconds;
    u64 nanoseconds;
} aegis_timespec_t;

s64 sys_clock_get(u64 clock_id, u64 timespec_ptr, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    if (!timespec_ptr) return AEGIS_EINVAL;
    aegis_timespec_t ts = {0};
    if (clock_id == AEGIS_CLOCK_MONOTONIC) {
        u64 ns = monotonic_nanoseconds();
        ts.seconds = ns / 1000000000ULL;
        ts.nanoseconds = ns % 1000000000ULL;
    } else if (clock_id == AEGIS_CLOCK_REALTIME) {
        if (!rtc_ready()) return AEGIS_ENOENT;
        ts.seconds = rtc_unix_seconds();
    } else return AEGIS_EINVAL;
    return copy_to_user_checked(timespec_ptr, &ts, sizeof(ts));
}

s64 sys_shutdown(u64 mode, u64 reason, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    u32 pid = process_getpid();
    const char *name = process_current_name();
    if (pid != 1U && (!name || !kernel_str_eq(name, "service-manager"))) return AEGIS_EPERM;
    service_supervisor_begin_shutdown();
    system_shutdown_request(pid, (u32)reason);
    if (mode == AEGIS_SHUTDOWN_REBOOT) system_reboot_commit();
    if (mode == AEGIS_SHUTDOWN_POWEROFF) system_shutdown_commit();
    return AEGIS_EINVAL;
}

s64 sys_socket(u64 type, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return net_socket_create(process_getpid(), (u32)type);
}
s64 sys_bind(u64 fd, u64 port, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    return net_socket_bind(process_getpid(), (int)fd, (u16)port);
}
s64 sys_listen(u64 fd, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return net_socket_listen(process_getpid(), (int)fd);
}
s64 sys_accept(u64 fd, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    network_poll();
    return net_socket_accept(process_getpid(), (int)fd);
}
s64 sys_connect(u64 fd, u64 ip, u64 port, u64 a3, u64 a4, u64 a5) {
    (void)a3; (void)a4; (void)a5;
    return net_socket_connect(process_getpid(), (int)fd, (u32)ip, (u16)port);
}
s64 sys_send(u64 fd, u64 buf, u64 len, u64 a3, u64 a4, u64 a5) {
    return sys_write(fd, buf, len, a3, a4, a5);
}
s64 sys_recv(u64 fd, u64 buf, u64 len, u64 a3, u64 a4, u64 a5) {
    network_poll();
    return sys_read(fd, buf, len, a3, a4, a5);
}
s64 sys_random(u64 buf, u64 len, u64 flags, u64 a3, u64 a4, u64 a5) {
    (void)a3; (void)a4; (void)a5;
    if (!buf || !len || len > 65536ULL || !user_write_range_ok(buf, len)) return AEGIS_EINVAL;
    u8 tmp[AEGIS_SYSCALL_IO_CHUNK];
    u64 done = 0;
    while (done < len) {
        u64 chunk = (len - done) < sizeof(tmp) ? (len - done) : sizeof(tmp);
        int rc = entropy_get(tmp, chunk, (flags & 1U) != 0U);
        if (rc != AEGIS_OK) return done ? (s64)done : (s64)rc;
        rc = copy_to_user_checked(buf + done, tmp, chunk);
        if (rc != AEGIS_OK) return rc;
        done += chunk;
    }
    return (s64)done;
}
