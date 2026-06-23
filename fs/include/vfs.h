/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "../../kernel/include/types.h"

#define VFS_PATH_MAX 256
#define VFS_NAME_MAX  64

typedef struct vnode vnode_t;
typedef struct vfs   vfs_t;

typedef struct vnode_ops {
    int (*read)(vnode_t *vn, u64 offset, void *buf, u64 len);
    int (*write)(vnode_t *vn, u64 offset, const void *buf, u64 len);
    int (*readdir)(vnode_t *vn, u32 index, char *name, vnode_t **out);
    int (*stat)(vnode_t *vn, void *stat_out);
} vnode_ops_t;

struct vfs {
    const char *name;
    int (*mount)(vfs_t *fs, const void *data, u64 size);
    vnode_ops_t *ops;
    void *priv;
};

struct vnode {
    u32          ino;
    u64          size;
    vfs_t       *vfs;
    vnode_ops_t *ops;
    void        *priv;
};

int      vfs_init(void);
int      vfs_mount(const char *path, vfs_t *fs);
int      vfs_umount(const char *path);
vnode_t *vfs_open(const char *path);
void     vfs_close(vnode_t *vn);
int      vfs_read(vnode_t *vn, u64 off, void *buf, u64 len);
int      vfs_write(vnode_t *vn, u64 off, const void *buf, u64 len);
int      vfs_readdir(vnode_t *vn, u32 index, char *name, vnode_t **out);
int      vfs_stat(vnode_t *vn, void *stat_out);

/* Early boot helpers used before a real userspace mount manager exists. */
u32         vfs_mount_count(void);
bool        vfs_is_mounted(const char *path);
const char *vfs_mount_fs_name(const char *path);
int         vfs_mount_bootstrap_pseudo(const char *path, const char *name);
int         vfs_bootstrap_selftest(void);
