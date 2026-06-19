/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/syscall.c
 * System call implementations.
 */

#include "types.h"
#include "memory.h"
#include "scheduler.h"
#include "syscalls.h"
#include "panic.h"

/* Forward declarations from other modules */
extern u32 process_getpid(void);
extern void process_exit(int code);
extern int vfs_read(void *vn, u64 off, void *buf, u64 len);
extern int vfs_write(void *vn, u64 off, const void *buf, u64 len);
extern void *vfs_open(const char *path);
extern void vfs_close(void *vn);

/* IPC forward declarations */
extern void *channel_create(u32 owner_tid);
extern void *channel_get(u32 id);

/* ---- Syscall implementations ---- */

s64 sys_read(u64 fd, u64 buf, u64 len, u64 a3, u64 a4, u64 a5) {
    (void)a3; (void)a4; (void)a5;
    if (!buf || len == 0) return AEGIS_EINVAL;
    /* TODO: fd table lookup; for now, return UART input for fd 0 */
    if (fd == 0) {
        extern int uart_getchar(void);
        u8 *ubuf = (u8 *)buf;
        u64 count = 0;
        for (u64 i = 0; i < len; i++) {
            int ch = uart_getchar();
            if (ch < 0) break;
            ubuf[i] = (u8)ch;
            count++;
        }
        return (s64)count;
    }
    return AEGIS_ENOENT;
}

s64 sys_write(u64 fd, u64 buf, u64 len, u64 a3, u64 a4, u64 a5) {
    (void)a3; (void)a4; (void)a5;
    if (!buf || len == 0) return AEGIS_EINVAL;
    /* fd 1 = stdout, fd 2 = stderr → UART output */
    if (fd == 1 || fd == 2) {
        extern void uart_putchar(char c);
        const char *s = (const char *)buf;
        for (u64 i = 0; i < len; i++) {
            uart_putchar(s[i]);
        }
        return (s64)len;
    }
    return AEGIS_ENOENT;
}

s64 sys_open(u64 path_ptr, u64 flags, u64 mode, u64 a3, u64 a4, u64 a5) {
    (void)flags; (void)mode; (void)a3; (void)a4; (void)a5;
    if (!path_ptr) return AEGIS_EINVAL;
    /* Placeholder: VFS open integration */
    void *vn = vfs_open((const char *)path_ptr);
    if (!vn) return AEGIS_ENOENT;
    /* Return a pseudo fd — real implementation would use a per-process fd table */
    return (s64)(uptr)vn;
}

s64 sys_close(u64 fd, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    if (fd <= 2) return AEGIS_OK; /* stdin/stdout/stderr never close */
    vfs_close((void *)(uptr)fd);
    return AEGIS_OK;
}

s64 sys_exit(u64 code, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    process_exit((int)code);
    /* Should not return */
    return 0;
}

s64 sys_getpid(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return (s64)process_getpid();
}

s64 sys_mmap(u64 addr, u64 length, u64 prot, u64 flags, u64 fd, u64 offset) {
    (void)prot; (void)flags; (void)fd; (void)offset;
    if (length == 0) return AEGIS_EINVAL;

    /* Allocate physical pages and map them */
    u64 pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    virt_addr_t va = addr ? addr : 0xFFFF000000000000UL; /* Default user range */

    for (u64 i = 0; i < pages; i++) {
        phys_addr_t pa = phys_alloc_page();
        if (pa == 0) return AEGIS_ENOMEM;
        virt_map(va + i * PAGE_SIZE, pa, PAGE_SIZE, 0x7); /* RWX */
    }
    return (s64)va;
}

s64 sys_munmap(u64 addr, u64 length, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    if (!addr || length == 0) return AEGIS_EINVAL;
    virt_unmap(addr, length);
    return AEGIS_OK;
}

s64 sys_send_msg(u64 channel_id, u64 data_ptr, u64 data_len, u64 a3, u64 a4, u64 a5) {
    (void)a3; (void)a4; (void)a5;
    if (!data_ptr || data_len == 0) return AEGIS_EINVAL;
    void *ch = channel_get((u32)channel_id);
    if (!ch) return AEGIS_ENOENT;
    /* Message queued — actual delivery handled by IPC subsystem */
    extern int ipc_send(u32 channel_id, const void *data, u64 len);
    return (s64)ipc_send((u32)channel_id, (const void *)data_ptr, data_len);
}

s64 sys_recv_msg(u64 channel_id, u64 buf_ptr, u64 buf_len, u64 a3, u64 a4, u64 a5) {
    (void)a3; (void)a4; (void)a5;
    if (!buf_ptr || buf_len == 0) return AEGIS_EINVAL;
    void *ch = channel_get((u32)channel_id);
    if (!ch) return AEGIS_ENOENT;
    extern int ipc_recv(u32 channel_id, void *buf, u64 max_len);
    return (s64)ipc_recv((u32)channel_id, (void *)buf_ptr, buf_len);
}

s64 sys_cap_grant(u64 target_pid, u64 cap_token, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    extern int process_grant_cap(u32 pid, cap_token_t cap);
    return (s64)process_grant_cap((u32)target_pid, (cap_token_t)cap_token);
}

s64 sys_cap_revoke(u64 target_pid, u64 cap_token, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    (void)target_pid; (void)cap_token;
    /* TODO: implement capability revocation */
    return AEGIS_OK;
}

s64 sys_yield(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    scheduler_yield();
    return AEGIS_OK;
}
