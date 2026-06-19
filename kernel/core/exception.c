/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/exception.c
 * Exception and syscall bridge handlers called from boot/arm64/exceptions.S.
 */

#include "panic.h"
#include "syscalls.h"
#include "types.h"

typedef struct {
    u64 x30;
    u64 x28;
    u64 x29;
    u64 x26;
    u64 x27;
    u64 x24;
    u64 x25;
    u64 x22;
    u64 x23;
    u64 x20;
    u64 x21;
    u64 x18;
    u64 x19;
    u64 x16;
    u64 x17;
    u64 x14;
    u64 x15;
    u64 x12;
    u64 x13;
    u64 x10;
    u64 x11;
    u64 x8;
    u64 x9;
    u64 x6;
    u64 x7;
    u64 x4;
    u64 x5;
    u64 x2;
    u64 x3;
    u64 x0;
    u64 x1;
} exception_frame_t;

void syscall_handler(void *regs) {
    exception_frame_t *f = (exception_frame_t *)regs;
    f->x0 = (u64)syscall_dispatch(f->x8, f->x0, f->x1, f->x2, f->x3, f->x4, f->x5);
}

void handle_exception(void *regs) {
    u64 esr;
    u64 elr;
    u64 far;

    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile("mrs %0, elr_el1" : "=r"(elr));
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));

    printk("[EXC] ESR=%p ELR=%p FAR=%p FRAME=%p\n",
           (void *)esr, (void *)elr, (void *)far, regs);
    PANIC("Unhandled exception");
}
