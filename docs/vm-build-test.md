# AegisOS VM Build/Test Notes

This build adds a real AArch64 kernel link path and a QEMU smoke-test harness.

## Host requirements

For local Linux testing:

```bash
sudo pacman -S --needed clang lld llvm qemu-system-aarch64 make
```

On Debian/Ubuntu, the QEMU package is usually `qemu-system-arm`.

## Build the kernel with Clang/LLD

```bash
tools/build/kernel-clang-aarch64.sh
```

Expected outputs:

```text
build/aegisos.elf
build/aegisos.bin
```

The ELF should report:

```text
ELF 64-bit LSB executable, ARM aarch64
```

## Run a QEMU smoke test

```bash
AEGISOS_QEMU_TIMEOUT=10 tools/qemu/smoke-aarch64.sh build/aegisos.bin
```

The smoke test intentionally uses a timeout because the kernel is not expected to
exit. A timeout after the configured interval is treated as a successful harness
run, provided QEMU started correctly.

## Notes

- `net/packet.c` now provides the fixed-size packet buffer pool required by the
  kernel network stack.
- `boot/arm64/start.S` and `boot/arm64/mmu.S` now use literal loads for symbols
  that may be outside AArch64 `adr` immediate range during kernel linking.
- This is still bring-up level. A successful QEMU launch proves the image loads;
  it does not prove production router behaviour.


## Strict UART banner smoke test

The VM harness now has an ultra-early PL011 UART marker in `boot/arm64/start.S`:

```text
[AegisOS] early boot: _start reached
```

This proves QEMU entered the AegisOS reset path before C/HAL initialisation. Run:

```bash
tools/build/kernel-clang-aarch64.sh
AEGISOS_QEMU_TIMEOUT=10 AEGISOS_QEMU_REQUIRE_BANNER=1 \
  tools/qemu/smoke-aarch64.sh build/aegisos.bin
```

If the kernel keeps running, QEMU timing out after the banner is normal. If the banner is missing, debug the QEMU load address, UART base, or `_start` path before debugging higher-level kernel code.

## Clock skew warning

If `make` reports files with modification times in the future after moving ZIPs between devices, normalise mtimes:

```bash
tools/dev/fix-clock-skew.sh
```

Then rebuild.
