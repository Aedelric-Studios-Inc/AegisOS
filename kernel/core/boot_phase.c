/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/boot_phase.c */

#include "boot_phase.h"
#include "panic.h"

static volatile boot_phase_t current_phase = BOOT_PHASE_RESET;
static u64 saved_dtb_ptr;

const char *boot_phase_name(boot_phase_t phase) {
    switch (phase) {
    case BOOT_PHASE_RESET:           return "reset";
    case BOOT_PHASE_EARLY_CONSOLE:   return "early-console";
    case BOOT_PHASE_PLATFORM:        return "platform";
    case BOOT_PHASE_MEMORY:          return "memory";
    case BOOT_PHASE_CORE:            return "core";
    case BOOT_PHASE_DEVICES:         return "devices";
    case BOOT_PHASE_KERNEL_SERVICES: return "kernel-services";
    case BOOT_PHASE_SECURITY:        return "security";
    case BOOT_PHASE_NETWORK:         return "network";
    case BOOT_PHASE_INIT:            return "init";
    case BOOT_PHASE_RUNNING:         return "running";
    case BOOT_PHASE_FAILED:          return "failed";
    default:                         return "unknown";
    }
}

void boot_phase_init(u64 dtb_ptr) {
    saved_dtb_ptr = dtb_ptr;
    current_phase = BOOT_PHASE_RESET;
}

void boot_phase_enter(boot_phase_t phase) {
    current_phase = phase;
    printk("[AegisOS:boot] phase=%s\n", boot_phase_name(phase));
}

void boot_phase_note(const char *message) {
    printk("[AegisOS:boot] %s\n", message);
}

void boot_phase_fail(const char *message) {
    current_phase = BOOT_PHASE_FAILED;
    printk("[AegisOS:boot] failed: %s\n", message);
}

boot_phase_t boot_phase_current(void) {
    return current_phase;
}

u64 boot_dtb_ptr(void) {
    return saved_dtb_ptr;
}
