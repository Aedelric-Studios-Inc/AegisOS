/* SPDX-License-Identifier: Proprietary */
/* AegisOS — fs/vfs.c */
#include "include/vfs.h"

int vfs_init(void)                              { return 0; }
int vfs_mount(const char *path, vfs_t *fs)      { (void)path; (void)fs; return 0; }
vnode_t *vfs_open(const char *path)             { (void)path; return NULL; }
void     vfs_close(vnode_t *vn)                 { (void)vn; }
int vfs_read(vnode_t *vn, u64 off, void *buf, u64 len) {
    if (!vn || !vn->ops || !vn->ops->read) return -1;
    return vn->ops->read(vn, off, buf, len);
}
int vfs_write(vnode_t *vn, u64 off, const void *buf, u64 len) {
    if (!vn || !vn->ops || !vn->ops->write) return -1;
    return vn->ops->write(vn, off, buf, len);
}
