/* SPDX-License-Identifier: Proprietary */
/* AegisOS — fs/initramfs.c
 * Initial RAM filesystem — provides an in-memory read-only filesystem
 * loaded from the CPIO archive embedded in the kernel image.
 * Used during early boot before persistent storage is available.
 */

#include "include/initramfs.h"
#include "include/vfs.h"
#include "../kernel/include/memory.h"
#include "../kernel/include/panic.h"

#define INITRAMFS_MAX_FILES  256
#define INITRAMFS_MAX_NAME   128

/* CPIO newc header (SVR4 portable format) */
typedef struct {
    char magic[6];      /* "070701" */
    char ino[8];
    char mode[8];
    char uid[8];
    char gid[8];
    char nlink[8];
    char mtime[8];
    char filesize[8];
    char devmajor[8];
    char devminor[8];
    char rdevmajor[8];
    char rdevminor[8];
    char namesize[8];
    char check[8];
} cpio_header_t;

typedef struct {
    char name[INITRAMFS_MAX_NAME];
    const u8 *data;
    u64 size;
    u32 mode;
    bool is_dir;
    bool valid;
} initramfs_entry_t;

static initramfs_entry_t files[INITRAMFS_MAX_FILES];
static u32 file_count = 0;
static bool mounted = false;

static u32 hex8_to_u32(const char *s) {
    u32 val = 0;
    for (int i = 0; i < 8; i++) {
        val <<= 4;
        char c = s[i];
        if (c >= '0' && c <= '9') val |= (u32)(c - '0');
        else if (c >= 'a' && c <= 'f') val |= (u32)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (u32)(c - 'A' + 10);
    }
    return val;
}

static u64 align4(u64 offset) {
    return (offset + 3) & ~3ULL;
}

/* VFS operations for initramfs */
static int initramfs_read(vnode_t *vn, u64 offset, void *buf, u64 len) {
    if (!vn || !vn->priv) return -1;
    initramfs_entry_t *entry = (initramfs_entry_t *)vn->priv;
    if (offset >= entry->size) return 0;
    u64 avail = entry->size - offset;
    u64 to_read = len < avail ? len : avail;
    const u8 *src = entry->data + offset;
    u8 *dst = (u8 *)buf;
    for (u64 i = 0; i < to_read; i++) dst[i] = src[i];
    return (int)to_read;
}

static int initramfs_readdir(vnode_t *vn, u32 index, char *name, vnode_t **out) {
    (void)vn; (void)out;
    if (index >= file_count) return -1;
    if (!files[index].valid) return -1;
    /* Copy filename */
    const char *src = files[index].name;
    int i = 0;
    while (src[i] && i < VFS_NAME_MAX - 1) { name[i] = src[i]; i++; }
    name[i] = '\0';
    return 0;
}

static vnode_ops_t initramfs_ops = {
    .read = initramfs_read,
    .write = NULL,
    .readdir = initramfs_readdir,
    .stat = NULL,
};

static vfs_t initramfs_vfs = {
    .name = "initramfs",
    .mount = NULL,
    .ops = &initramfs_ops,
    .priv = NULL,
};

int initramfs_mount(const void *data, u64 size) {
    if (!data || size == 0) return -1;
    if (mounted) return -1;

    const u8 *ptr = (const u8 *)data;
    const u8 *end = ptr + size;
    file_count = 0;

    while (ptr + sizeof(cpio_header_t) <= end && file_count < INITRAMFS_MAX_FILES) {
        const cpio_header_t *hdr = (const cpio_header_t *)ptr;

        /* Verify magic */
        if (hdr->magic[0] != '0' || hdr->magic[1] != '7' ||
            hdr->magic[2] != '0' || hdr->magic[3] != '7' ||
            hdr->magic[4] != '0' || hdr->magic[5] != '1') {
            break;
        }

        u32 namesize = hex8_to_u32(hdr->namesize);
        u32 filesize = hex8_to_u32(hdr->filesize);
        u32 mode = hex8_to_u32(hdr->mode);

        const char *name = (const char *)(ptr + sizeof(cpio_header_t));

        /* Check for TRAILER marker */
        if (namesize == 11) {
            bool is_trailer = true;
            const char *trailer = "TRAILER!!!";
            for (int i = 0; i < 10; i++) {
                if (name[i] != trailer[i]) { is_trailer = false; break; }
            }
            if (is_trailer) break;
        }

        /* Skip "." entry */
        if (namesize > 1 || name[0] != '.') {
            initramfs_entry_t *entry = &files[file_count];
            /* Copy name (skip leading ./) */
            const char *n = name;
            if (n[0] == '.' && n[1] == '/') n += 2;
            int i = 0;
            while (*n && i < INITRAMFS_MAX_NAME - 1) { entry->name[i++] = *n++; }
            entry->name[i] = '\0';

            u64 data_offset = align4(sizeof(cpio_header_t) + namesize);
            entry->data = ptr + data_offset;
            entry->size = filesize;
            entry->mode = mode;
            entry->is_dir = (mode & 0040000) != 0;
            entry->valid = true;
            file_count++;
        }

        /* Advance to next entry */
        u64 header_plus_name = align4(sizeof(cpio_header_t) + namesize);
        u64 total = header_plus_name + align4(filesize);
        ptr += total;
    }

    /* Mount at / */
    vfs_mount("/", &initramfs_vfs);
    mounted = true;
    return 0;
}

/* Lookup a file by path within initramfs */
initramfs_entry_t *initramfs_lookup(const char *path) {
    if (!path) return NULL;
    /* Skip leading / */
    if (*path == '/') path++;

    for (u32 i = 0; i < file_count; i++) {
        if (!files[i].valid) continue;
        const char *a = path;
        const char *b = files[i].name;
        bool match = true;
        while (*a && *b) {
            if (*a != *b) { match = false; break; }
            a++; b++;
        }
        if (match && *a == '\0' && *b == '\0') {
            return &files[i];
        }
    }
    return NULL;
}

u32 initramfs_file_count(void) {
    return file_count;
}
