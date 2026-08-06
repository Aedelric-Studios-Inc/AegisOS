/* SPDX-License-Identifier: Proprietary */
/* AegisOS — boot/arm64/start.S
 * AArch64 reset entry point.
 */

.section ".text.boot.start", "ax"
.global _start
.extern _vectors

_start:
    /*
     * AArch64/QEMU Image header.
     *
     * QEMU's virt -kernel path is happiest when the image begins with an
     * ARM64 Image-style header. The first instruction is still executable,
     * so bare raw loaders branch straight into the real reset path too.
     */
    b       _aegisos_reset_entry      /* code0 */
    .word   0                         /* code1 */
    .quad   0x00080000                /* text_offset from RAM base */
    .quad   _end - _start             /* image_size */
    .quad   0                         /* flags */
    .quad   0                         /* reserved */
    .quad   0                         /* reserved */
    .quad   0                         /* reserved */
    .word   0x644d5241                /* magic: ARM64_IMAGE_MAGIC */
    .word   0                         /* reserved */
    .balign 64

_aegisos_reset_entry:
    /* Preserve x0: QEMU/Linux ARM64 boot protocol passes the FDT pointer here. */
    mov     x20, x0

    /* Disable interrupts. */
    msr     daifset, #0xf

    /*
     * Ultra-early UART marker for QEMU/bring-up.
     * This uses the QEMU virt PL011 at 0x09000000 directly so the VM smoke
     * test can prove that the CPU reached _start before C/HAL init.
     */
#ifdef AEGISOS_QEMU_SMOKE
    /*
     * QEMU smoke marker order matters.  Print to PL011 first, then try
     * semihosting.  Some QEMU builds can trap/hang on semihosting before
     * returning, so UART must be attempted before HLT #0xf000.
     */
    bl      early_uart_init
    adr     x0, early_boot_banner
    bl      early_uart_puts

    /* QEMU-only semihosting marker. Do not enable for hardware images. */
    adr     x1, early_semihost_banner
    mov     x0, #4                   /* SYS_WRITE0 */
    hlt     #0xf000

    /*
     * v20: do not stop here.  Entry proof is valuable, but the next bring-up
     * milestone is proving that _start reaches kernel_main and the boot phase
     * coordinator.  The C smoke path decides when to halt/exit.
     */
#endif

    bl      early_uart_init
    adr     x0, early_boot_banner
    bl      early_uart_puts

    /* Zero BSS */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
    cmp     x0, x1
    beq     1f
    mov     x2, #0
0:  str     x2, [x0], #8
    cmp     x0, x1
    blo     0b
1:
    /* Set up stack */
    ldr     x0, =_stack_top
    mov     sp, x0

    /* Install exception vector table */
    ldr     x0, =_vectors
    msr     vbar_el1, x0
    isb

    /* Jump to kernel main. x0 = preserved FDT/DTB pointer. */
    mov     x0, x20
    bl      kernel_main

    /* Should never return */
halt:
    wfe
    b       halt


/* Early PL011 UART helpers for QEMU virt machine. Clobbers x0-x4. */
early_uart_init:
    movz    x1, #0x0000
    movk    x1, #0x0900, lsl #16     /* x1 = 0x09000000 */
    str     wzr, [x1, #0x30]          /* UARTCR = 0, disable */
    mov     w2, #13                   /* IBRD for 24MHz / 115200 */
    str     w2, [x1, #0x24]
    mov     w2, #21                   /* FBRD */
    str     w2, [x1, #0x28]
    mov     w2, #0x70                 /* 8N1 + FIFO */
    str     w2, [x1, #0x2c]
    mov     w2, #0x301                /* RXE | TXE | UARTEN */
    str     w2, [x1, #0x30]
    ret

early_uart_puts:
    movz    x1, #0x0000
    movk    x1, #0x0900, lsl #16     /* x1 = 0x09000000 */
1:  ldrb    w2, [x0], #1
    cbz     w2, 4f
    movz    w4, #0xffff              /* bounded wait: never hang forever */
2:  ldr     w3, [x1, #0x18]           /* UARTFR */
    tbz     w3, #5, 3f                /* TX FIFO not full */
    subs    w4, w4, #1
    b.ne    2b
3:  str     w2, [x1, #0x00]           /* UARTDR */
    b       1b
4:  ret

early_boot_banner:
    .asciz "[AegisOS] early boot: _start reached\n"

early_semihost_banner:
    .asciz "[AegisOS] semihost: _start reached\n"

