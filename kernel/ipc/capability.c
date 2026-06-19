/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/ipc/capability.c
 * Capability token management.
 */

#include "types.h"
#include "memory.h"

#define MAX_CAPS 4096

typedef struct {
    cap_token_t token;
    u32         owner_tid;
    u32         target_tid;
    u64         rights;
    bool        valid;
} capability_t;

static capability_t cap_table[MAX_CAPS];
static u32          next_cap_idx = 1;

cap_token_t cap_create(u32 owner_tid, u32 target_tid, u64 rights) {
    if (next_cap_idx >= MAX_CAPS) return NULL_CAP;
    capability_t *c = &cap_table[next_cap_idx];
    c->token      = (cap_token_t)next_cap_idx;
    c->owner_tid  = owner_tid;
    c->target_tid = target_tid;
    c->rights     = rights;
    c->valid      = true;
    return c->token;
}

s64 sys_cap_grant(u64 cap_tok, u64 target_tid, u64 rights, u64 a3, u64 a4, u64 a5) {
    (void)a3; (void)a4; (void)a5;
    /* TODO: validate caller owns cap, create delegated capability */
    (void)cap_tok; (void)target_tid; (void)rights;
    return AEGIS_OK;
}

s64 sys_cap_revoke(u64 cap_tok, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    u64 idx = cap_tok;
    if (idx == 0 || idx >= MAX_CAPS) return AEGIS_EINVAL;
    cap_table[idx].valid = false;
    return AEGIS_OK;
}
