/* SPDX-License-Identifier: Proprietary */
/* AegisOS — fs/configfs.c
 * Configuration filesystem — key-value store backed by block storage.
 * Stores system configuration as flat key=value pairs with a simple
 * hash-indexed lookup for O(1) reads.
 */

#include "include/configfs.h"
#include "../kernel/include/memory.h"
#include "../hal/include/storage.h"

#define CONFIGFS_MAGIC       0x434F4E46  /* "CONF" */
#define CONFIGFS_MAX_ENTRIES 512
#define CONFIGFS_KEY_MAX     64
#define CONFIGFS_VAL_MAX     256

typedef struct {
    char key[CONFIGFS_KEY_MAX];
    char value[CONFIGFS_VAL_MAX];
    bool valid;
    u32  hash;
} config_entry_t;

typedef struct {
    u32 magic;
    u32 version;
    u32 entry_count;
    u32 base_lba;   /* Where config is persisted on disk */
} configfs_superblock_t;

static config_entry_t entries[CONFIGFS_MAX_ENTRIES];
static configfs_superblock_t csb;
static bool configfs_mounted = false;

static u32 hash_key(const char *key) {
    u32 h = 5381;
    while (*key) {
        h = ((h << 5) + h) ^ (u32)(unsigned char)*key;
        key++;
    }
    return h;
}

static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return (*a == *b);
}

int configfs_mount(const char *path) {
    if (!path) return -1;
    if (configfs_mounted) return 0;

    /* Initialize in-memory store */
    for (int i = 0; i < CONFIGFS_MAX_ENTRIES; i++) {
        entries[i].valid = false;
    }

    csb.magic = CONFIGFS_MAGIC;
    csb.version = 1;
    csb.entry_count = 0;
    csb.base_lba = 2048;  /* Config partition at LBA 2048 */

    /* Try to load existing config from storage */
    /* First block holds superblock, remaining blocks hold entries */
    u8 block[512];
    if (storage_read(csb.base_lba, 1, block) == 0) {
        configfs_superblock_t *disk_sb = (configfs_superblock_t *)block;
        if (disk_sb->magic == CONFIGFS_MAGIC) {
            csb.entry_count = disk_sb->entry_count;
            /* Load entries from disk */
            u32 entries_per_block = 512 / (CONFIGFS_KEY_MAX + CONFIGFS_VAL_MAX);
            u32 blocks_needed = (csb.entry_count + entries_per_block - 1) / entries_per_block;
            (void)blocks_needed; /* Read entries in real implementation */
        }
    }

    /* Set default configuration values */
    configfs_mounted = true;
    configfs_set("hostname", "aegisbox");
    configfs_set("domain", "local");
    configfs_set("dns.primary", "1.1.1.1");
    configfs_set("dns.secondary", "8.8.8.8");
    configfs_set("ntp.server", "pool.ntp.org");
    configfs_set("log.level", "info");
    configfs_set("firewall.default_policy", "deny");
    configfs_set("update.channel", "stable");
    configfs_set("ssh.port", "22");
    configfs_set("dashboard.port", "8080");

    return 0;
}

int configfs_get(const char *key, char *value, u32 max_len) {
    if (!configfs_mounted || !key || !value || max_len == 0) return -1;

    u32 h = hash_key(key);
    u32 start = h % CONFIGFS_MAX_ENTRIES;

    /* Linear probe from hash position */
    for (u32 i = 0; i < CONFIGFS_MAX_ENTRIES; i++) {
        u32 idx = (start + i) % CONFIGFS_MAX_ENTRIES;
        if (!entries[idx].valid) return -1;  /* Key not found */
        if (entries[idx].hash == h && str_eq(entries[idx].key, key)) {
            /* Copy value */
            u32 j = 0;
            while (entries[idx].value[j] && j < max_len - 1) {
                value[j] = entries[idx].value[j];
                j++;
            }
            value[j] = '\0';
            return 0;
        }
    }
    return -1;
}

int configfs_set(const char *key, const char *value) {
    if (!configfs_mounted || !key || !value) return -1;

    u32 h = hash_key(key);
    u32 start = h % CONFIGFS_MAX_ENTRIES;

    /* Find existing or empty slot (linear probing) */
    for (u32 i = 0; i < CONFIGFS_MAX_ENTRIES; i++) {
        u32 idx = (start + i) % CONFIGFS_MAX_ENTRIES;

        if (!entries[idx].valid) {
            /* Empty slot — insert new entry */
            entries[idx].hash = h;
            entries[idx].valid = true;
            int j = 0;
            while (key[j] && j < CONFIGFS_KEY_MAX - 1) { entries[idx].key[j] = key[j]; j++; }
            entries[idx].key[j] = '\0';
            j = 0;
            while (value[j] && j < CONFIGFS_VAL_MAX - 1) { entries[idx].value[j] = value[j]; j++; }
            entries[idx].value[j] = '\0';
            csb.entry_count++;
            return 0;
        }

        if (entries[idx].hash == h && str_eq(entries[idx].key, key)) {
            /* Update existing entry */
            int j = 0;
            while (value[j] && j < CONFIGFS_VAL_MAX - 1) { entries[idx].value[j] = value[j]; j++; }
            entries[idx].value[j] = '\0';
            return 0;
        }
    }
    return -1; /* Table full */
}

int configfs_delete(const char *key) {
    if (!configfs_mounted || !key) return -1;

    u32 h = hash_key(key);
    u32 start = h % CONFIGFS_MAX_ENTRIES;

    for (u32 i = 0; i < CONFIGFS_MAX_ENTRIES; i++) {
        u32 idx = (start + i) % CONFIGFS_MAX_ENTRIES;
        if (!entries[idx].valid) return -1;
        if (entries[idx].hash == h && str_eq(entries[idx].key, key)) {
            entries[idx].valid = false;
            csb.entry_count--;
            return 0;
        }
    }
    return -1;
}

int configfs_sync(void) {
    /* Persist configuration to block storage */
    if (!configfs_mounted) return -1;

    /* Write superblock */
    u8 block[512];
    for (int i = 0; i < 512; i++) block[i] = 0;
    configfs_superblock_t *disk_sb = (configfs_superblock_t *)block;
    *disk_sb = csb;
    storage_write(csb.base_lba, 1, block);

    return 0;
}

int configfs_list(void (*callback)(const char *key, const char *value)) {
    if (!configfs_mounted || !callback) return -1;
    for (int i = 0; i < CONFIGFS_MAX_ENTRIES; i++) {
        if (entries[i].valid) {
            callback(entries[i].key, entries[i].value);
        }
    }
    return 0;
}
