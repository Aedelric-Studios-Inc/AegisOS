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

static const char *esr_ec_name(u64 ec) {
    switch (ec) {
        case 0x00: return "Unknown reason";
        case 0x01: return "WFI/WFE trap";
        case 0x07: return "FP/SIMD access trap";
        case 0x11: return "SVC AArch32";
        case 0x15: return "SVC AArch64";
        case 0x18: return "MSR/MRS/System instruction trap";
        case 0x20: return "Instruction abort (lower EL)";
        case 0x21: return "Instruction abort (current EL)";
        case 0x22: return "PC alignment fault";
        case 0x24: return "Data abort (lower EL)";
        case 0x25: return "Data abort (current EL)";
        case 0x26: return "SP alignment fault";
        case 0x2c: return "FP exception AArch64";
        case 0x2f: return "SError interrupt";
        case 0x30: return "Breakpoint (lower EL)";
        case 0x31: return "Breakpoint (current EL)";
        case 0x3c: return "BRK instruction AArch64";
        default:   return "Reserved/implementation-defined";
    }
}

static const char *abort_fsc_name(u64 fsc) {
    switch (fsc) {
        case 0x00: return "Address size fault, level 0";
        case 0x01: return "Address size fault, level 1";
        case 0x02: return "Address size fault, level 2";
        case 0x03: return "Address size fault, level 3";
        case 0x04: return "Translation fault, level 0";
        case 0x05: return "Translation fault, level 1";
        case 0x06: return "Translation fault, level 2";
        case 0x07: return "Translation fault, level 3";
        case 0x09: return "Access flag fault, level 1";
        case 0x0a: return "Access flag fault, level 2";
        case 0x0b: return "Access flag fault, level 3";
        case 0x0d: return "Permission fault, level 1";
        case 0x0e: return "Permission fault, level 2";
        case 0x0f: return "Permission fault, level 3";
        case 0x10: return "Synchronous external abort";
        case 0x11: return "Synchronous tag check fault";
        case 0x15: return "Synchronous external abort, level 1";
        case 0x16: return "Synchronous external abort, level 2";
        case 0x17: return "Synchronous external abort, level 3";
        case 0x18: return "Synchronous parity/ECC error";
        case 0x1c: return "Synchronous parity/ECC error, level 0";
        case 0x1d: return "Synchronous parity/ECC error, level 1";
        case 0x1e: return "Synchronous parity/ECC error, level 2";
        case 0x1f: return "Synchronous parity/ECC error, level 3";
        case 0x21: return "Alignment fault";
        case 0x22: return "Debug event";
        default:   return "Other/implementation-defined";
    }
}

static void dump_exception_decode(u64 esr) {
    u64 ec  = (esr >> 26) & 0x3f;
    u64 il  = (esr >> 25) & 0x1;
    u64 iss = esr & 0x01ffffff;

    printk("[EXC] EC=0x%p (%s) IL=%p ISS=0x%p\n",
           (void *)ec, esr_ec_name(ec), (void *)il, (void *)iss);

    if (ec == 0x15 || ec == 0x11 || ec == 0x3c) {
        u64 imm16 = iss & 0xffff;
        printk("[EXC] IMM16=0x%p\n", (void *)imm16);
    }

    if (ec == 0x24 || ec == 0x25 || ec == 0x20 || ec == 0x21) {
        u64 fsc = iss & 0x3f;
        u64 wnr = (iss >> 6) & 0x1;
        u64 isv = (iss >> 24) & 0x1;
        printk("[EXC] FSC=0x%p (%s) WnR=%p ISV=%p\n",
               (void *)fsc, abort_fsc_name(fsc), (void *)wnr, (void *)isv);
    }
}

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
    dump_exception_decode(esr);
    PANIC("Unhandled exception");
}
