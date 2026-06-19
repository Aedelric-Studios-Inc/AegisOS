/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/ipc/channel.c
 * IPC channel management.
 */

#include "types.h"
#include "memory.h"

#define MAX_CHANNELS 1024

typedef struct channel {
    u32  id;
    u32  owner_tid;
    bool open;
} channel_t;

static channel_t channel_table[MAX_CHANNELS];
static u32       next_channel_id = 1;

channel_t *channel_create(u32 owner_tid) {
    if (next_channel_id >= MAX_CHANNELS) return NULL;
    channel_t *ch = &channel_table[next_channel_id];
    ch->id        = next_channel_id++;
    ch->owner_tid = owner_tid;
    ch->open      = true;
    return ch;
}

channel_t *channel_get(u32 id) {
    if (id == 0 || id >= MAX_CHANNELS) return NULL;
    if (!channel_table[id].open) return NULL;
    return &channel_table[id];
}

void channel_close(u32 id) {
    if (id < MAX_CHANNELS) channel_table[id].open = false;
}
