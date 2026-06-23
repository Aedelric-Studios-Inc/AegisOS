/* SPDX-License-Identifier: Proprietary */
/* AegisOS — fs/logfs.c
 * Log-structured filesystem — append-only log storage optimized for
 * system logs, audit trails, and diagnostic output. Written to block
 * storage sequentially with CRC32 integrity checks.
 */

#include "include/logfs.h"
#include "../kernel/include/memory.h"
#include "../hal/include/storage.h"

#define LOGFS_MAGIC        0x4C4F4746  /* "LOGF" */
#define LOGFS_MAX_TAG      32
#define LOGFS_MAX_MSG      448
#define LOGFS_ENTRY_SIZE   512  /* Matches block size */
#define LOGFS_MAX_ENTRIES  65536
#define LOGFS_RING_SIZE    8192  /* In-memory ring buffer entries */

typedef struct {
    u32 magic;
    u32 seq;
    u64 timestamp;
    char tag[LOGFS_MAX_TAG];
    char msg[LOGFS_MAX_MSG];
    u32 crc32;
    u32 reserved;
} __attribute__((packed)) logfs_entry_t;

typedef struct {
    u32 magic;
    u32 version;
    u32 total_entries;
    u32 write_head;      /* Next LBA to write to */
    u32 base_lba;        /* Starting LBA on disk */
    u32 max_entries;
    u64 create_time;
} logfs_superblock_t;

static logfs_entry_t ring_buffer[LOGFS_RING_SIZE];
static u32 ring_head = 0;
static u32 ring_count = 0;
static u32 sequence = 0;
static bool logfs_mounted = false;
static logfs_superblock_t sb;

static u32 crc32_simple(const u8 *data, u32 len) {
    u32 crc = 0xFFFFFFFF;
    for (u32 i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

int logfs_mount(const char *path) {
    if (!path) return -1;
    if (logfs_mounted) return 0;

    /* Initialize ring buffer */
    for (u32 i = 0; i < LOGFS_RING_SIZE; i++) {
        ring_buffer[i].magic = 0;
    }
    ring_head = 0;
    ring_count = 0;
    sequence = 0;

    /* Initialize superblock */
    sb.magic = LOGFS_MAGIC;
    sb.version = 1;
    sb.total_entries = 0;
    sb.write_head = 0;
    sb.base_lba = 1024; /* Log partition starts at LBA 1024 */
    sb.max_entries = LOGFS_MAX_ENTRIES;
    sb.create_time = 0;

    logfs_mounted = true;
    return 0;
}

int logfs_append(const char *tag, const char *msg) {
    if (!logfs_mounted) return -1;
    if (!tag || !msg) return -1;

    logfs_entry_t *entry = &ring_buffer[ring_head];
    entry->magic = LOGFS_MAGIC;
    entry->seq = sequence++;
    entry->timestamp = 0; /* Would use timer_get_ticks() */

    /* Copy tag */
    int i = 0;
    while (tag[i] && i < LOGFS_MAX_TAG - 1) { entry->tag[i] = tag[i]; i++; }
    entry->tag[i] = '\0';

    /* Copy message */
    i = 0;
    while (msg[i] && i < LOGFS_MAX_MSG - 1) { entry->msg[i] = msg[i]; i++; }
    entry->msg[i] = '\0';

    /* Compute CRC over the entry (excluding crc32 field itself) */
    entry->crc32 = crc32_simple((const u8 *)entry,
                                 (u32)((u8 *)&entry->crc32 - (u8 *)entry));
    entry->reserved = 0;

    /* Advance ring */
    ring_head = (ring_head + 1) % LOGFS_RING_SIZE;
    if (ring_count < LOGFS_RING_SIZE) ring_count++;

    sb.total_entries++;
    sb.write_head = (sb.write_head + 1) % sb.max_entries;

    /* Persist to storage asynchronously (write-behind) */
    u32 lba = sb.base_lba + ((sb.write_head - 1) % sb.max_entries);
    storage_write(lba, 1, entry);

    return 0;
}

int logfs_read_last(u32 n, char *buf, u32 buf_len) {
    if (!logfs_mounted || !buf || buf_len == 0) return -1;
    if (n > ring_count) n = ring_count;
    if (n == 0) return 0;

    u32 pos = 0;
    u32 start_idx;
    if (ring_count < LOGFS_RING_SIZE) {
        start_idx = (ring_count >= n) ? (ring_count - n) : 0;
    } else {
        start_idx = (ring_head + LOGFS_RING_SIZE - n) % LOGFS_RING_SIZE;
    }

    for (u32 i = 0; i < n && pos < buf_len - 1; i++) {
        u32 idx = (start_idx + i) % LOGFS_RING_SIZE;
        logfs_entry_t *entry = &ring_buffer[idx];
        if (entry->magic != LOGFS_MAGIC) continue;

        /* Format: "[tag] msg\n" */
        buf[pos++] = '[';
        for (int j = 0; entry->tag[j] && pos < buf_len - 3; j++) {
            buf[pos++] = entry->tag[j];
        }
        buf[pos++] = ']';
        buf[pos++] = ' ';
        for (int j = 0; entry->msg[j] && pos < buf_len - 2; j++) {
            buf[pos++] = entry->msg[j];
        }
        buf[pos++] = '\n';
    }
    buf[pos] = '\0';
    return (int)pos;
}

int logfs_flush(void) {
    /* Force flush all pending entries to block storage */
    if (!logfs_mounted) return -1;
    /* In a full implementation, this would drain the write-behind queue */
    return 0;
}

u32 logfs_entry_count(void) {
    return sb.total_entries;
}
