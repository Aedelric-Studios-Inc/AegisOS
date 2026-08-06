/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/ipc/channel.c
 * IPC channel management with v57 named native-service channels.
 */

#include "types.h"
#include "ipc_channel.h"

#define MAX_CHANNELS 1024
#define MAX_NAMED_CHANNELS 32

typedef struct channel {
    u32  id;
    u32  owner_id;
    bool open;
} channel_t;

typedef struct named_channel {
    bool used;
    u32 channel_id;
    char name[AEGIS_NAMED_CHANNEL_NAME_MAX];
} named_channel_t;

static channel_t channel_table[MAX_CHANNELS];
static named_channel_t named_channels[MAX_NAMED_CHANNELS];
static u32 next_channel_id = 1;

extern int ipc_create_queue(u32 channel_id);

static bool name_eq(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static void name_copy(char *dst, const char *src) {
    u32 i = 0;
    while (src && src[i] && i < AEGIS_NAMED_CHANNEL_NAME_MAX - 1U) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void channel_init(void) {
    for (u32 i = 0; i < MAX_CHANNELS; i++) {
        channel_table[i].id = 0;
        channel_table[i].owner_id = 0;
        channel_table[i].open = false;
    }
    for (u32 i = 0; i < MAX_NAMED_CHANNELS; i++) {
        named_channels[i].used = false;
        named_channels[i].channel_id = 0;
        named_channels[i].name[0] = '\0';
    }
    next_channel_id = 1;
}

channel_t *channel_create(u32 owner_id) {
    if (next_channel_id >= MAX_CHANNELS) return NULL;
    channel_t *ch = &channel_table[next_channel_id];
    ch->id = next_channel_id++;
    ch->owner_id = owner_id;
    ch->open = true;
    return ch;
}

channel_t *channel_get(u32 id) {
    if (id == 0 || id >= MAX_CHANNELS || !channel_table[id].open) return NULL;
    return &channel_table[id];
}

void channel_close(u32 id) {
    if (id < MAX_CHANNELS) channel_table[id].open = false;
}

u32 channel_id_from_handle(void *handle) {
    channel_t *ch = (channel_t *)handle;
    if (!ch || !ch->open) return 0;
    return ch->id;
}

int channel_open_named(const char *name, u32 owner_id) {
    if (!name || !name[0]) return AEGIS_EINVAL;

    for (u32 i = 0; i < MAX_NAMED_CHANNELS; i++) {
        if (named_channels[i].used && name_eq(named_channels[i].name, name)) {
            return (int)named_channels[i].channel_id;
        }
    }

    u32 free_index = MAX_NAMED_CHANNELS;
    for (u32 i = 0; i < MAX_NAMED_CHANNELS; i++) {
        if (!named_channels[i].used) {
            free_index = i;
            break;
        }
    }
    if (free_index == MAX_NAMED_CHANNELS) return AEGIS_ENOMEM;

    channel_t *ch = channel_create(owner_id);
    u32 id = channel_id_from_handle(ch);
    if (!ch || id == 0) return AEGIS_ENOMEM;
    int queue = ipc_create_queue(id);
    if (queue < 0) {
        channel_close(id);
        return queue;
    }

    named_channels[free_index].used = true;
    named_channels[free_index].channel_id = id;
    name_copy(named_channels[free_index].name, name);
    return (int)id;
}

const char *channel_name(u32 id) {
    for (u32 i = 0; i < MAX_NAMED_CHANNELS; i++) {
        if (named_channels[i].used && named_channels[i].channel_id == id) {
            return named_channels[i].name;
        }
    }
    return NULL;
}
