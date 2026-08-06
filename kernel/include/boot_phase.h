/* SPDX-License-Identifier: Proprietary */
#pragma once
/* AegisOS boot phase coordinator.
 *
 * Keep early kernel bring-up deterministic: each subsystem is attached to a
 * named phase, phase transitions are logged, and QEMU smoke tests can prove
 * progression without relying on a visible serial banner.
 */

#include "types.h"

typedef enum boot_phase {
    BOOT_PHASE_RESET = 0,
    BOOT_PHASE_EARLY_CONSOLE,
    BOOT_PHASE_PLATFORM,
    BOOT_PHASE_MEMORY,
    BOOT_PHASE_CORE,
    BOOT_PHASE_DEVICES,
    BOOT_PHASE_KERNEL_SERVICES,
    BOOT_PHASE_SECURITY,
    BOOT_PHASE_NETWORK,
    BOOT_PHASE_INIT,
    BOOT_PHASE_RUNNING,
    BOOT_PHASE_FAILED,
} boot_phase_t;

void boot_phase_init(u64 dtb_ptr);
void boot_phase_enter(boot_phase_t phase);
void boot_phase_note(const char *message);
void boot_phase_fail(const char *message);
boot_phase_t boot_phase_current(void);
const char *boot_phase_name(boot_phase_t phase);
u64 boot_dtb_ptr(void);
