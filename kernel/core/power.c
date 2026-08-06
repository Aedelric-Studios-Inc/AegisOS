/* SPDX-License-Identifier: Proprietary */
#include "power.h"
#include "process.h"
#include "block_storage.h"

static volatile bool g_requested;
static u32 g_requester;
static u32 g_reason;
extern void printk(const char *fmt, ...);

void system_shutdown_request(u32 requester_pid, u32 reason) {
    if (g_requested) return;
    g_requester = requester_pid;
    g_reason = reason;
    g_requested = true;
    (void)process_terminate_all_except(requester_pid, 15);
}
bool system_shutdown_requested(void) { return g_requested; }
u32 system_shutdown_requester(void) { return g_requester; }
u32 system_shutdown_reason(void) { return g_reason; }

static void psci_call(u64 fn) {
    register u64 x0 __asm__("x0") = fn;
    __asm__ volatile("hvc #0" : "+r"(x0) :: "x1", "x2", "x3", "memory");
}
void system_shutdown_commit(void) {
    printk("[AegisOS:power] clean shutdown requester=%u reason=%u\n", g_requester, g_reason);
    const aegis_block_registry_t *st = block_storage_state();
    if (st) for (u32 i = 0; i < st->device_count; i++) { const aegis_block_device_t *d = block_storage_device(i); if (d) (void)block_storage_flush(d->id); }
    psci_call(0x84000008ULL); /* PSCI SYSTEM_OFF */
    for (;;) __asm__ volatile("wfi");
}
void system_reboot_commit(void) {
    printk("[AegisOS:power] clean reboot requester=%u reason=%u\n", g_requester, g_reason);
    psci_call(0x84000009ULL); /* PSCI SYSTEM_RESET */
    for (;;) __asm__ volatile("wfi");
}
