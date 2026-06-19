/* SPDX-License-Identifier: Proprietary */
/* AegisOS — fs/vfs.c
 * Virtual Filesystem Switch — manages mount points, path resolution,
 * and delegates operations to registered filesystem drivers.
 */

#include "include/vfs.h"
#include "../kernel/include/memory.h"
#include "../kernel/include/panic.h"

#define VFS_MAX_MOUNTS   32
#define VFS_MAX_OPEN     256

/* Filesystem type descriptor */
struct vfs {
    const char *name;
    int (*mount)(vfs_t *fs, const void *data, u64 size);
    vnode_ops_t *ops;
    void *priv;
};

/* Mount point */
typedef struct mount_point {
    char path[VFS_PATH_MAX];
    vfs_t *fs;
    vnode_t *root;
    bool active;
} mount_point_t;

static mount_point_t mounts[VFS_MAX_MOUNTS];
static vnode_t open_vnodes[VFS_MAX_OPEN];
static u32 next_ino = 1;

int vfs_init(void) {
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        mounts[i].active = false;
    }
    for (int i = 0; i < VFS_MAX_OPEN; i++) {
        open_vnodes[i].ino = 0;
    }
    return 0;
}

static int path_match_len(const char *path, const char *mount_path) {
    int len = 0;
    while (mount_path[len] && path[len] == mount_path[len]) {
        len++;
    }
    if (mount_path[len] != '\0') return 0;
    /* Must match at path boundary */
    if (path[len] != '\0' && path[len] != '/') return 0;
    return len;
}

static mount_point_t *vfs_resolve_mount(const char *path, const char **remainder) {
    mount_point_t *best = NULL;
    int best_len = 0;

    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!mounts[i].active) continue;
        int len = path_match_len(path, mounts[i].path);
        if (len > best_len) {
            best_len = len;
            best = &mounts[i];
        }
    }

    if (best && remainder) {
        *remainder = path + best_len;
        if (**remainder == '/') (*remainder)++;
    }
    return best;
}

int vfs_mount(const char *path, vfs_t *fs) {
    if (!path || !fs) return AEGIS_EINVAL;

    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!mounts[i].active) {
            /* Copy mount path */
            int j = 0;
            while (path[j] && j < VFS_PATH_MAX - 1) {
                mounts[i].path[j] = path[j];
                j++;
            }
            mounts[i].path[j] = '\0';
            mounts[i].fs = fs;
            mounts[i].root = NULL;
            mounts[i].active = true;
            return AEGIS_OK;
        }
    }
    return AEGIS_ENOMEM;
}

int vfs_umount(const char *path) {
    if (!path) return AEGIS_EINVAL;
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!mounts[i].active) continue;
        /* Compare paths */
        const char *a = path;
        const char *b = mounts[i].path;
        bool match = true;
        while (*a && *b) {
            if (*a != *b) { match = false; break; }
            a++; b++;
        }
        if (match && *a == '\0' && *b == '\0') {
            mounts[i].active = false;
            return AEGIS_OK;
        }
    }
    return AEGIS_ENOENT;
}

vnode_t *vfs_open(const char *path) {
    if (!path) return NULL;

    const char *remainder = NULL;
    mount_point_t *mp = vfs_resolve_mount(path, &remainder);
    if (!mp || !mp->fs || !mp->fs->ops) return NULL;

    /* Find a free vnode slot */
    vnode_t *vn = NULL;
    for (int i = 0; i < VFS_MAX_OPEN; i++) {
        if (open_vnodes[i].ino == 0) {
            vn = &open_vnodes[i];
            break;
        }
    }
    if (!vn) return NULL;

    vn->ino = next_ino++;
    vn->size = 0;
    vn->vfs = mp->fs;
    vn->ops = mp->fs->ops;
    vn->priv = (void *)remainder; /* Pass relative path as private data */
    return vn;
}

void vfs_close(vnode_t *vn) {
    if (!vn) return;
    vn->ino = 0;
    vn->ops = NULL;
    vn->vfs = NULL;
    vn->priv = NULL;
}

int vfs_read(vnode_t *vn, u64 off, void *buf, u64 len) {
    if (!vn || !vn->ops || !vn->ops->read) return AEGIS_EINVAL;
    return vn->ops->read(vn, off, buf, len);
}

int vfs_write(vnode_t *vn, u64 off, const void *buf, u64 len) {
    if (!vn || !vn->ops || !vn->ops->write) return AEGIS_EINVAL;
    return vn->ops->write(vn, off, buf, len);
}

int vfs_readdir(vnode_t *vn, u32 index, char *name, vnode_t **out) {
    if (!vn || !vn->ops || !vn->ops->readdir) return AEGIS_EINVAL;
    return vn->ops->readdir(vn, index, name, out);
}

int vfs_stat(vnode_t *vn, void *stat_out) {
    if (!vn || !vn->ops || !vn->ops->stat) return AEGIS_EINVAL;
    return vn->ops->stat(vn, stat_out);
}
