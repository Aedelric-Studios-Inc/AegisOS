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
    /* Validate that cap_tok refers to a real, valid capability */
    if (cap_tok == 0 || cap_tok >= MAX_CAPS) return AEGIS_EINVAL;
    capability_t *src = &cap_table[cap_tok];
    if (!src->valid) return AEGIS_EINVAL;

    /* Caller must own the capability or be the kernel (tid 0) */
    /* (thread-id check omitted for now; real impl checks current task) */

    /* Rights may only be a subset of the source capability's rights */
    u64 delegated_rights = rights & src->rights;

    /* Create a new delegated capability for the target */
    cap_token_t new_tok = cap_create(src->owner_tid, (u32)target_tid, delegated_rights);
    if (new_tok == NULL_CAP) return AEGIS_ENOMEM;

    return (s64)new_tok;
}

s64 cap_revoke(cap_token_t tok) {
    u64 idx = tok;
    if (idx == 0 || idx >= MAX_CAPS) return AEGIS_EINVAL;
    cap_table[idx].valid = false;
    return AEGIS_OK;
}
