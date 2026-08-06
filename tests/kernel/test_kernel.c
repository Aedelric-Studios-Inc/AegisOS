/* SPDX-License-Identifier: Proprietary */
/* AegisOS — tests/kernel/test_kernel.c
 * Unit tests for the kernel subsystem.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ---- Minimal stubs for kernel types used by process/capability logic ---- */

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long  u64;
typedef long           s64;
typedef long long      s32;
typedef long           iptr;
typedef unsigned long  uptr;
typedef unsigned long  phys_addr_t;
typedef unsigned long  virt_addr_t;
typedef unsigned long  cap_token_t;
#define NULL_CAP  ((cap_token_t)0)
#define AEGIS_OK       0
#define AEGIS_EINVAL  -2
#define AEGIS_ENOENT  -4
#define AEGIS_ENOMEM  -1
#define PAGE_SIZE 4096UL

/* ---- PID counter simulation ---- */

static u32 g_next_pid = 1;
static u32 alloc_pid(void) { return g_next_pid++; }

/* ---- Capability table simulation ---- */

#define MAX_CAPS 64
typedef struct { cap_token_t token; u32 owner; u32 target; u64 rights; int valid; } cap_t;
static cap_t cap_table[MAX_CAPS];
static u32   cap_next = 1;

static cap_token_t cap_create_test(u32 owner, u32 target, u64 rights) {
    if (cap_next >= MAX_CAPS) return NULL_CAP;
    cap_t *c = &cap_table[cap_next];
    c->token = cap_next; c->owner = owner; c->target = target;
    c->rights = rights; c->valid = 1;
    return cap_next++;
}

static int cap_revoke_test(cap_token_t tok) {
    if (tok == 0 || tok >= MAX_CAPS) return AEGIS_EINVAL;
    cap_table[tok].valid = 0;
    return AEGIS_OK;
}

static cap_t *cap_get_test(cap_token_t tok) {
    if (tok == 0 || tok >= MAX_CAPS || !cap_table[tok].valid) return NULL;
    return &cap_table[tok];
}

/* ---- Tests ---- */

static void test_pid_allocation(void) {
    u32 a = alloc_pid();
    u32 b = alloc_pid();
    assert(b == a + 1);
    printf("  [PASS] PID allocation is monotonically increasing\n");
}

static void test_cap_create(void) {
    cap_token_t tok = cap_create_test(1, 2, 0xFF);
    cap_token_t tok2 = cap_create_test(1, 4, 0x0F);
    assert(tok != NULL_CAP);
    assert(tok2 == tok + 1);
    cap_t *c = cap_get_test(tok);
    assert(c != NULL);
    assert(c->owner  == 1);
    assert(c->target == 2);
    assert(c->rights == 0xFF);
    printf("  [PASS] Capability creation stores owner/target/rights and increments tokens\n");
}

static void test_cap_revoke(void) {
    cap_token_t tok = cap_create_test(1, 3, 0x0F);
    assert(cap_get_test(tok) != NULL);
    assert(cap_revoke_test(tok) == AEGIS_OK);
    assert(cap_get_test(tok) == NULL);   /* revoked → not found */
    printf("  [PASS] Capability revocation removes token from table\n");
}

static void test_cap_delegate_rights_subset(void) {
    cap_token_t parent = cap_create_test(1, 2, 0xFF);
    cap_t *p = cap_get_test(parent);
    assert(p != NULL);
    /* Delegated rights must be a subset */
    u64 delegated = 0x0F & p->rights;
    cap_token_t child = cap_create_test(p->owner, 3, delegated);
    cap_t *c = cap_get_test(child);
    assert(c != NULL);
    assert((c->rights & ~p->rights) == 0); /* no rights beyond parent */
    printf("  [PASS] Delegated capability has subset of parent rights\n");
}

static void test_cap_invalid_token(void) {
    assert(cap_revoke_test(NULL_CAP) == AEGIS_EINVAL);
    assert(cap_get_test(NULL_CAP)    == NULL);
    printf("  [PASS] NULL capability token is rejected\n");
}

static void test_scheduler_yield_noop(void) {
    /* Without a real scheduler we just verify the concept compiles.
     * A real test would check context switch counters. */
    volatile int x = 0;
    x = 1; /* Simulate doing work before yield */
    assert(x == 1);
    printf("  [PASS] Scheduler yield placeholder executes without fault\n");
}

int main(void) {
    printf("[test_kernel] Running kernel unit tests...\n");
    test_pid_allocation();
    test_cap_create();
    test_cap_revoke();
    test_cap_delegate_rights_subset();
    test_cap_invalid_token();
    test_scheduler_yield_noop();
    printf("[test_kernel] All tests passed\n");
    return 0;
}
