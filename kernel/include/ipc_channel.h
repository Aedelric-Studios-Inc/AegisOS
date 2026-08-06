/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "types.h"

#define AEGIS_NAMED_CHANNEL_NAME_MAX 32U

typedef struct channel channel_t;

void channel_init(void);
channel_t *channel_create(u32 owner_id);
channel_t *channel_get(u32 id);
void channel_close(u32 id);
u32 channel_id_from_handle(void *handle);
int channel_open_named(const char *name, u32 owner_id);
const char *channel_name(u32 id);
