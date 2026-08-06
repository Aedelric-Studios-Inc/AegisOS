/* SPDX-License-Identifier: Proprietary */
#include "fd_table.h"
#include "vfs.h"

#define FD_PROCESS_SLOTS 256U

typedef struct fd_entry {
    bool used;
    bool readable;
    bool writable;
    void *object;
    u64 offset;
} fd_entry_t;

static fd_entry_t g_fd[FD_PROCESS_SLOTS][AEGIS_FD_MAX_PER_PROCESS];

static void zero_bytes(void *ptr, u64 len) {
    u8 *p = (u8 *)ptr;
    for (u64 i = 0; i < len; i++) p[i] = 0;
}

void fd_table_init(void) { zero_bytes(g_fd, sizeof(g_fd)); }

int fd_install(u32 pid, void *object, bool readable, bool writable) {
    if (!pid || pid >= FD_PROCESS_SLOTS || !object) return AEGIS_EINVAL;
    for (u32 i = 0; i < AEGIS_FD_MAX_PER_PROCESS; i++) {
        if (g_fd[pid][i].used) continue;
        g_fd[pid][i].used = true;
        g_fd[pid][i].readable = readable;
        g_fd[pid][i].writable = writable;
        g_fd[pid][i].object = object;
        g_fd[pid][i].offset = 0;
        return (int)i + AEGIS_FD_FIRST;
    }
    return AEGIS_ENOSPC;
}

void *fd_get(u32 pid, int fd, bool for_write, u64 **offset_out) {
    if (!pid || pid >= FD_PROCESS_SLOTS || fd < AEGIS_FD_FIRST) return NULL;
    u32 index = (u32)(fd - AEGIS_FD_FIRST);
    if (index >= AEGIS_FD_MAX_PER_PROCESS) return NULL;
    fd_entry_t *e = &g_fd[pid][index];
    if (!e->used || (for_write ? !e->writable : !e->readable)) return NULL;
    if (offset_out) *offset_out = &e->offset;
    return e->object;
}

int fd_close(u32 pid, int fd) {
    if (!pid || pid >= FD_PROCESS_SLOTS || fd < AEGIS_FD_FIRST) return AEGIS_EINVAL;
    u32 index = (u32)(fd - AEGIS_FD_FIRST);
    if (index >= AEGIS_FD_MAX_PER_PROCESS || !g_fd[pid][index].used) return AEGIS_ENOENT;
    vfs_close(g_fd[pid][index].object);
    zero_bytes(&g_fd[pid][index], sizeof(g_fd[pid][index]));
    return AEGIS_OK;
}

void fd_close_all(u32 pid) {
    if (!pid || pid >= FD_PROCESS_SLOTS) return;
    for (u32 i = 0; i < AEGIS_FD_MAX_PER_PROCESS; i++) {
        if (!g_fd[pid][i].used) continue;
        vfs_close(g_fd[pid][i].object);
        zero_bytes(&g_fd[pid][i], sizeof(g_fd[pid][i]));
    }
}
