#!/usr/bin/env bash
# Build and boot tiny standalone AArch64 semihosting/UART tests.
# This confirms the host QEMU entry path independent of AegisOS.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
mkdir -p build/qemu-sanity

cat > build/qemu-sanity/sanity_semihost_exit.S <<'ASM'
.section ".text.boot.start", "ax"
.global _start
_start:
    msr daifset, #0xf
    adr x1, msg_semi
    mov x0, #4              // SYS_WRITE0
    hlt #0xf000
    movz x1, #0x0026       // ADP_Stopped_ApplicationExit
    movk x1, #0x0002, lsl #16
    mov x0, #0x18           // SYS_EXIT
    hlt #0xf000
1:  wfe
    b 1b
msg_semi:
    .asciz "[AegisOS sanity] semihost reached\n"
ASM

cat > build/qemu-sanity/sanity_payload.inc <<'ASM'
    msr daifset, #0xf
    // Write to QEMU virt PL011 UART0 at 0x09000000.  Do not depend on
    // existing firmware UART setup; initialise a minimal 115200-ish config.
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    str wzr, [x1, #0x30]    // CR = 0
    mov w2, #13
    str w2, [x1, #0x24]     // IBRD
    mov w2, #21
    str w2, [x1, #0x28]     // FBRD
    mov w2, #0x70
    str w2, [x1, #0x2c]     // LCRH 8N1/FIFO
    mov w2, #0x301
    str w2, [x1, #0x30]     // UARTEN|TXE|RXE
    adr x0, msg_uart
    bl puts
    adr x1, msg_semi
    mov x0, #4              // SYS_WRITE0
    hlt #0xf000
    movz x1, #0x0026       // ADP_Stopped_ApplicationExit
    movk x1, #0x0002, lsl #16
    mov x0, #0x18           // SYS_EXIT
    hlt #0xf000
2:  wfe
    b 2b
puts:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
3:  ldrb w2, [x0], #1
    cbz w2, 5f
    movz w4, #0xffff
4:  ldr w3, [x1, #0x18]
    tbz w3, #5, 6f          // wait until TXFF clear, bounded
    subs w4, w4, #1
    b.ne 4b
6:  str w2, [x1]
    b 3b
5:  ret
msg_uart:
    .asciz "[AegisOS sanity] UART reached\n"
msg_semi:
    .asciz "[AegisOS sanity] semihost reached\n"
ASM

cat > build/qemu-sanity/sanity_bare.S <<'ASM'
.section ".text.boot.start", "ax"
.global _start
_start:
    b 1f
    .balign 64
1:
ASM
cat build/qemu-sanity/sanity_payload.inc >> build/qemu-sanity/sanity_bare.S

cat > build/qemu-sanity/sanity_image.S <<'ASM'
.section ".text.boot.start", "ax"
.global _start
_start:
    b 1f
    .word 0
    .quad 0x00080000
    .quad _end - _start
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .word 0x644d5241
    .word 0
    .balign 64
1:
ASM
cat build/qemu-sanity/sanity_payload.inc >> build/qemu-sanity/sanity_image.S
printf '%s\n' '_end:' >> build/qemu-sanity/sanity_image.S

cat > build/qemu-sanity/link-bios.ld <<'LD'
OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)
SECTIONS {
  . = 0x00000000;
  .text.boot : { KEEP(*(.text.boot.start)) *(.text*) }
  .rodata : ALIGN(16) { *(.rodata*) }
  .data : ALIGN(16) { *(.data*) }
  .bss : ALIGN(16) { *(.bss*) *(COMMON) }
}
LD

cat > build/qemu-sanity/link-ram.ld <<'LD'
OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)
SECTIONS {
  . = 0x40080000;
  .text.boot : { KEEP(*(.text.boot.start)) *(.text*) }
  .rodata : ALIGN(16) { *(.rodata*) }
  .data : ALIGN(16) { *(.data*) }
  .bss : ALIGN(16) { *(.bss*) *(COMMON) }
}
LD

build_one() {
  local src="$1"
  local out="$2"
  local linker="$3"
  clang --target=aarch64-none-elf -ffreestanding -nostdlib -c "build/qemu-sanity/${src}.S" -o "build/qemu-sanity/${out}.o"
  ld.lld -T "build/qemu-sanity/${linker}" -o "build/qemu-sanity/${out}.elf" "build/qemu-sanity/${out}.o"
  llvm-objcopy -O binary "build/qemu-sanity/${out}.elf" "build/qemu-sanity/${out}.bin"
}

build_one sanity_semihost_exit sanity_semihost_exit_bios link-bios.ld
build_one sanity_semihost_exit sanity_semihost_exit_ram link-ram.ld
build_one sanity_bare sanity_bios link-bios.ld
build_one sanity_bare sanity_ram link-ram.ld
build_one sanity_image sanity_image link-ram.ld
file build/qemu-sanity/sanity_semihost_exit_bios.elf build/qemu-sanity/sanity_semihost_exit_bios.bin build/qemu-sanity/sanity_semihost_exit_ram.elf build/qemu-sanity/sanity_semihost_exit_ram.bin build/qemu-sanity/sanity_bios.elf build/qemu-sanity/sanity_bios.bin build/qemu-sanity/sanity_ram.elf build/qemu-sanity/sanity_ram.bin build/qemu-sanity/sanity_image.elf build/qemu-sanity/sanity_image.bin || true

try_sanity() {
  local mode="$1"
  local image="$2"
  local tag="$3"
  echo "== QEMU sanity: $tag =="
  AEGISOS_QEMU_TIMEOUT="${AEGISOS_QEMU_TIMEOUT:-5}" \
  AEGISOS_QEMU_REQUIRE_BANNER=1 \
  AEGISOS_QEMU_UART_BANNER='[AegisOS sanity] UART reached' \
  AEGISOS_QEMU_SEMIHOST_BANNER='[AegisOS sanity] semihost reached' \
  AEGISOS_QEMU_LOG="build/qemu-sanity/smoke-$tag.log" \
  AEGISOS_QEMU_DEBUG_LOG="build/qemu-sanity/debug-$tag.log" \
  AEGISOS_QEMU_LOAD_MODE="$mode" \
    tools/qemu/smoke-aarch64.sh "$image"
}

# v18: start with the proven path. The HMP probe showed -kernel image mode
# executing the QEMU stub at 0x40000000 and entering the payload at 0x40080000.
if try_sanity kernel build/qemu-sanity/sanity_image.bin kernel-image-bin; then exit 0; fi
if try_sanity kernel build/qemu-sanity/sanity_image.elf kernel-image-elf; then exit 0; fi

# Firmware/bios sanity is useful for proving semihosting, but stdout banners may
# be swallowed on this host. The smoke harness now accepts trace proof.
if try_sanity bios build/qemu-sanity/sanity_semihost_exit_bios.bin semihost-bios-exit; then exit 0; fi
if try_sanity bios build/qemu-sanity/sanity_bios.bin bios-bare; then exit 0; fi

# Loader modes can overlap QEMU's generated DTB at 0x40000000-0x40100000 on
# this QEMU 11 virt machine. Keep them optional diagnostics only.
if [[ "${AEGISOS_QEMU_TRY_UNSAFE_LOADERS:-0}" == "1" ]]; then
  if try_sanity loader-raw build/qemu-sanity/sanity_semihost_exit_ram.bin semihost-loader-raw-exit; then exit 0; fi
  if try_sanity loader-elf build/qemu-sanity/sanity_semihost_exit_ram.elf semihost-loader-elf-exit; then exit 0; fi
  if try_sanity loader-raw build/qemu-sanity/sanity_ram.bin loader-raw-bare; then exit 0; fi
  if try_sanity loader-elf build/qemu-sanity/sanity_ram.elf loader-elf-bare; then exit 0; fi
  if try_sanity bios build/qemu-sanity/sanity_image.bin bios-image; then exit 0; fi
else
  echo "note: skipped unsafe loader diagnostics; set AEGISOS_QEMU_TRY_UNSAFE_LOADERS=1 to run them."
fi

echo "error: QEMU sanity failed on proven -kernel/bios paths." >&2
echo "upload these logs:" >&2
for f in \
  smoke-semihost-bios-exit.log debug-semihost-bios-exit.log \
  smoke-semihost-loader-raw-exit.log debug-semihost-loader-raw-exit.log \
  smoke-semihost-loader-elf-exit.log debug-semihost-loader-elf-exit.log \
  smoke-bios-bare.log debug-bios-bare.log \
  smoke-loader-raw-bare.log debug-loader-raw-bare.log \
  smoke-loader-elf-bare.log debug-loader-elf-bare.log \
  smoke-kernel-image-bin.log debug-kernel-image-bin.log \
  smoke-kernel-image-elf.log debug-kernel-image-elf.log \
  smoke-bios-image.log debug-bios-image.log; do
  echo "  build/qemu-sanity/$f" >&2
done
exit 3
