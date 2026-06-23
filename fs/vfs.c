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

static bool str_eq(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int pseudo_read(vnode_t *vn, u64 offset, void *buf, u64 len) {
    (void)vn;
    if (!buf) return AEGIS_EINVAL;
    if (offset != 0 || len == 0) return 0;
    return 0;
}

static int pseudo_readdir(vnode_t *vn, u32 index, char *name, vnode_t **out) {
    (void)vn;
    (void)out;
    if (!name) return AEGIS_EINVAL;
    if (index != 0) return AEGIS_ENOENT;
    name[0] = '\0';
    return AEGIS_ENOENT;
}

static vnode_ops_t pseudo_ops = {
    .read = pseudo_read,
    .write = NULL,
    .readdir = pseudo_readdir,
    .stat = NULL,
};

static vfs_t bootstrap_devfs = {
    .name = "devfs",
    .mount = NULL,
    .ops = &pseudo_ops,
    .priv = NULL,
};

static vfs_t bootstrap_procfs = {
    .name = "procfs",
    .mount = NULL,
    .ops = &pseudo_ops,
    .priv = NULL,
};

static vfs_t bootstrap_tmpfs = {
    .name = "tmpfs",
    .mount = NULL,
    .ops = &pseudo_ops,
    .priv = NULL,
};

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
    if (mount_path && mount_path[0] == '/' && mount_path[1] == '\0') {
        return path && path[0] == '/' ? 1 : 0;
    }
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
    if (!path || !fs || path[0] != '/') return AEGIS_EINVAL;

    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (mounts[i].active && str_eq(mounts[i].path, path)) {
            return AEGIS_EBUSY;
        }
    }

    if (fs->mount) {
        int rc = fs->mount(fs, NULL, 0);
        if (rc != AEGIS_OK) return rc;
    }

    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!mounts[i].active) {
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


u32 vfs_mount_count(void) {
    u32 count = 0;
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (mounts[i].active) count++;
    }
    return count;
}

bool vfs_is_mounted(const char *path) {
    if (!path) return false;
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (mounts[i].active && str_eq(mounts[i].path, path)) return true;
    }
    return false;
}

const char *vfs_mount_fs_name(const char *path) {
    if (!path) return NULL;
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (mounts[i].active && str_eq(mounts[i].path, path)) {
            return mounts[i].fs ? mounts[i].fs->name : NULL;
        }
    }
    return NULL;
}

int vfs_mount_bootstrap_pseudo(const char *path, const char *name) {
    if (!path || !name) return AEGIS_EINVAL;
    if (str_eq(name, "devfs")) return vfs_mount(path, &bootstrap_devfs);
    if (str_eq(name, "procfs")) return vfs_mount(path, &bootstrap_procfs);
    if (str_eq(name, "tmpfs")) return vfs_mount(path, &bootstrap_tmpfs);
    return AEGIS_EINVAL;
}

int vfs_bootstrap_selftest(void) {
    if (!vfs_is_mounted("/")) return AEGIS_ENOENT;
    if (!vfs_is_mounted("/dev")) return AEGIS_ENOENT;
    if (!vfs_is_mounted("/proc")) return AEGIS_ENOENT;
    if (vfs_mount_count() < 3U) return AEGIS_EINVAL;

    const char *root_name = vfs_mount_fs_name("/");
    const char *dev_name = vfs_mount_fs_name("/dev");
    const char *proc_name = vfs_mount_fs_name("/proc");
    if (!root_name || !dev_name || !proc_name) return AEGIS_EINVAL;

    vnode_t *root = vfs_open("/");
    vnode_t *dev = vfs_open("/dev");
    vnode_t *proc = vfs_open("/proc");
    if (!root || !dev || !proc) {
        if (root) vfs_close(root);
        if (dev) vfs_close(dev);
        if (proc) vfs_close(proc);
        return AEGIS_EINVAL;
    }

    if (!root->vfs || !dev->vfs || !proc->vfs) {
        vfs_close(proc);
        vfs_close(dev);
        vfs_close(root);
        return AEGIS_EINVAL;
    }

    vfs_close(proc);
    vfs_close(dev);
    vfs_close(root);
    return AEGIS_OK;
}
