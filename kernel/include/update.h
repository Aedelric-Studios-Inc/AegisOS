/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "types.h"

#define AEGIS_UPDATE_HASH_BYTES 32U
#define AEGIS_UPDATE_VERSION_MAX 32U

typedef enum aegis_update_slot { AEGIS_UPDATE_SLOT_A=0, AEGIS_UPDATE_SLOT_B=1, AEGIS_UPDATE_SLOT_NONE=0xff } aegis_update_slot_t;
typedef struct aegis_update_state {
    bool initialised;
    bool metadata_valid;
    bool pending;
    bool rollback_required;
    aegis_update_slot_t active_slot;
    aegis_update_slot_t pending_slot;
    u32 boot_attempts;
    u32 max_boot_attempts;
    u64 generation;
    char active_version[AEGIS_UPDATE_VERSION_MAX];
    char pending_version[AEGIS_UPDATE_VERSION_MAX];
    u8 active_hash[AEGIS_UPDATE_HASH_BYTES];
    u8 pending_hash[AEGIS_UPDATE_HASH_BYTES];
} aegis_update_state_t;

int update_init(void);
int update_stage(aegis_update_slot_t slot,const char*version,const u8 hash[32]);
int update_note_boot_attempt(void);
int update_mark_boot_success(void);
int update_request_rollback(void);
const aegis_update_state_t *update_state(void);
