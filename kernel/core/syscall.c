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

#define AEGIS_SYSCALL_PATH_MAX     128UL
#define AEGIS_SYSCALL_MSG_MAX      256UL
#define AEGIS_SYSCALL_IO_CHUNK     128UL

/* Forward declarations from other modules */
extern u32 process_getpid(void);
extern void process_exit(int code);
extern int vfs_read(void *vn, u64 off, void *buf, u64 len);
extern int vfs_write(void *vn, u64 off, const void *buf, u64 len);
extern void *vfs_open(const char *path);
extern void vfs_close(void *vn);

/* IPC/capability forward declarations */
extern void *channel_get(u32 id);
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
    uptr p = (uptr)ptr;
    return range_inside(p, len, AEGIS_USER_TEXT_BASE,  AEGIS_USER_TEXT_SIZE)  ||
           range_inside(p, len, AEGIS_USER_HEAP_BASE,  AEGIS_USER_HEAP_SIZE)  ||
           range_inside(p, len, AEGIS_USER_IPC_BASE,   AEGIS_USER_IPC_SIZE)   ||
           range_inside(p, len, AEGIS_USER_STACK_TOP - AEGIS_USER_STACK_SIZE,
                        AEGIS_USER_STACK_SIZE);
}

static bool user_write_range_ok(u64 ptr, u64 len) {
    uptr p = (uptr)ptr;
    return range_inside(p, len, AEGIS_USER_HEAP_BASE,  AEGIS_USER_HEAP_SIZE)  ||
           range_inside(p, len, AEGIS_USER_IPC_BASE,   AEGIS_USER_IPC_SIZE)   ||
           range_inside(p, len, AEGIS_USER_STACK_TOP - AEGIS_USER_STACK_SIZE,
                        AEGIS_USER_STACK_SIZE);
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
    return AEGIS_ENOENT;
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
    return AEGIS_ENOENT;
}

s64 sys_open(u64 path_ptr, u64 flags, u64 mode, u64 a3, u64 a4, u64 a5) {
    (void)flags; (void)mode; (void)a3; (void)a4; (void)a5;
    char path[AEGIS_SYSCALL_PATH_MAX];
    int rc = copy_string_from_user(path, sizeof(path), path_ptr);
    if (rc != AEGIS_OK) return rc;
    void *vn = vfs_open(path);
    if (!vn) return AEGIS_ENOENT;
    return (s64)(uptr)vn;
}

s64 sys_close(u64 fd, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    if (fd <= 2) return AEGIS_OK;
    vfs_close((void *)(uptr)fd);
    return AEGIS_OK;
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
    void *ch = channel_get((u32)channel_id);
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
    void *ch = channel_get((u32)channel_id);
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
    (void)a2; (void)a3; (void)a4; (void)a5;
    char path[AEGIS_SYSCALL_PATH_MAX];
    int rc = copy_string_from_user(path, sizeof(path), path_ptr);
    if (rc != AEGIS_OK) return rc;
    return userland_request_process_launch_by_path(path, flags);
}

s64 sys_waitpid(u64 pid, u64 exit_code_ptr, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    if (pid == 0) return AEGIS_EINVAL;
    int code = 0;
    int rc = process_wait((u32)pid, &code);
    if (rc == AEGIS_OK && exit_code_ptr) {
        int cr = copy_to_user_checked(exit_code_ptr, &code, sizeof(code));
        if (cr != AEGIS_OK) return cr;
    }
    return (s64)rc;
}

s64 sys_getppid(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return 0;
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
