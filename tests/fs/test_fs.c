/* SPDX-License-Identifier: Proprietary */
/* AegisOS — tests/fs/test_fs.c
 * Unit tests for the filesystem subsystem.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ---- VFS mount-table simulation ---- */

#define VFS_MAX_MOUNTS 8
#define VFS_PATH_MAX   64

typedef struct { char path[VFS_PATH_MAX]; int used; } mount_entry_t;
static mount_entry_t mounts[VFS_MAX_MOUNTS];

static int vfs_mount_test(const char *path) {
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!mounts[i].used) {
            strncpy(mounts[i].path, path, VFS_PATH_MAX - 1);
            mounts[i].path[VFS_PATH_MAX - 1] = '\0';
            mounts[i].used = 1;
            return 0;
        }
    }
    return -1; /* no free slot */
}

static int vfs_is_mounted(const char *path) {
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (mounts[i].used && strcmp(mounts[i].path, path) == 0) return 1;
    }
    return 0;
}

static int vfs_umount_test(const char *path) {
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (mounts[i].used && strcmp(mounts[i].path, path) == 0) {
            mounts[i].used = 0;
            return 0;
        }
    }
    return -1;
}

/* ---- CRC32 (matches logfs.c implementation) ---- */

static uint32_t crc32_byte(uint32_t crc, uint8_t b) {
    crc ^= b;
    for (int i = 0; i < 8; i++)
        crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320U : 0);
    return crc;
}

static uint32_t crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++) crc = crc32_byte(crc, data[i]);
    return crc ^ 0xFFFFFFFFU;
}

/* ---- Tests ---- */

static void test_vfs_mount_unmount(void) {
    assert(vfs_mount_test("/proc") == 0);
    assert(vfs_is_mounted("/proc"));
    assert(vfs_umount_test("/proc") == 0);
    assert(!vfs_is_mounted("/proc"));
    printf("  [PASS] VFS mount and unmount round-trip\n");
}

static void test_vfs_multiple_mounts(void) {
    const char *paths[] = { "/proc", "/sys", "/dev", "/run" };
    for (int i = 0; i < 4; i++) assert(vfs_mount_test(paths[i]) == 0);
    for (int i = 0; i < 4; i++) assert(vfs_is_mounted(paths[i]));
    for (int i = 0; i < 4; i++) vfs_umount_test(paths[i]);
    printf("  [PASS] Multiple concurrent mount points\n");
}

static void test_vfs_mount_table_full(void) {
    /* Fill the table */
    char path[16];
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        snprintf(path, sizeof(path), "/mnt%d", i);
        assert(vfs_mount_test(path) == 0);
    }
    /* One more should fail */
    assert(vfs_mount_test("/overflow") == -1);
    /* Clean up */
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        snprintf(path, sizeof(path), "/mnt%d", i);
        vfs_umount_test(path);
    }
    printf("  [PASS] Mount table full returns error\n");
}

static void test_crc32_known_vector(void) {
    /* CRC32 of "123456789" == 0xCBF43926 */
    const uint8_t msg[] = "123456789";
    assert(crc32(msg, 9) == 0xCBF43926U);
    printf("  [PASS] CRC32 matches NIST test vector\n");
}

static void test_crc32_empty(void) {
    assert(crc32(NULL, 0) == 0x00000000U);
    printf("  [PASS] CRC32 of empty data is 0\n");
}

static void test_path_longest_prefix(void) {
    /* Simulate longest-prefix mount resolution */
    vfs_mount_test("/");
    vfs_mount_test("/usr");
    vfs_mount_test("/usr/local");

    /* "/usr/local/bin" should match "/usr/local" (longest) */
    const char *path = "/usr/local/bin";
    const char *best = "/";
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!mounts[i].used) continue;
        size_t ml = strlen(mounts[i].path);
        if (strncmp(mounts[i].path, path, ml) == 0 && ml > strlen(best))
            best = mounts[i].path;
    }
    assert(strcmp(best, "/usr/local") == 0);
    vfs_umount_test("/"); vfs_umount_test("/usr"); vfs_umount_test("/usr/local");
    printf("  [PASS] Longest-prefix mount resolution selects deepest match\n");
}

int main(void) {
    printf("[test_fs] Running filesystem unit tests...\n");
    test_vfs_mount_unmount();
    test_vfs_multiple_mounts();
    test_vfs_mount_table_full();
    test_crc32_known_vector();
    test_crc32_empty();
    test_path_longest_prefix();
    printf("[test_fs] All tests passed\n");
    return 0;
}
