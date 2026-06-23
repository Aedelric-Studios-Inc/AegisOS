# AegisOS QEMU Strict Entry Test v8

v8 separates two different questions:

1. **Did QEMU reach `_start` at all?**
2. **Is the PL011 UART route working?**

The v7 test only watched UART. If UART routing was wrong, it looked like boot
failed even when the kernel image was otherwise valid.

v8 adds a QEMU-only semihosting marker behind `AEGISOS_QEMU_SMOKE=1`. This
marker proves `_start` execution even if PL011 output is not working yet.

## Strict smoke-test command

```bash
tools/dev/fix-clock-skew.sh
AEGISOS_QEMU_SMOKE=1 tools/build/kernel-clang-aarch64.sh

AEGISOS_QEMU_TIMEOUT=10 AEGISOS_QEMU_REQUIRE_BANNER=1 \
  tools/qemu/smoke-aarch64.sh build/aegisos.elf
```

Expected good output:

```text
[AegisOS] semihost: _start reached
qemu smoke test saw semihosting banner: [AegisOS] semihost: _start reached
qemu timed out after 10s; acceptable for a non-exiting kernel.
```

If the semihost banner appears but the UART banner does not, the CPU entry path
is alive and the next bug is PL011 serial configuration/routing.

If neither banner appears, the next bug is QEMU image loading, entry address,
link address, or an immediate pre-banner exception.

## Hardware note

Do not enable `AEGISOS_QEMU_SMOKE=1` for hardware/router builds. The
semihosting `hlt #0xf000` is for QEMU diagnostics only.
