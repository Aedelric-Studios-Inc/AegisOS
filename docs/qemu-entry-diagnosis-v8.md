# QEMU Entry Diagnosis v8

The v7 strict test failed because the raw `aegisos.bin` image did not begin at
`_start`. The linker placed another `.text.boot` helper (`mmu_init`) before the
reset entry, so QEMU's raw binary loader entered the first byte of the file and
never reached the UART banner.

v8 fixes this by:

- moving `boot/arm64/start.S` into `.text.boot.start`
- forcing `.text.boot.start` to the first bytes of the linked image
- keeping ordinary `.text.boot` helpers after the reset entry
- adding QEMU-only semihosting diagnostics behind `AEGISOS_QEMU_SMOKE=1`
- making the QEMU smoke script accept either semihosting or UART proof

## Run this on Arch

```bash
tools/dev/fix-clock-skew.sh
AEGISOS_QEMU_SMOKE=1 tools/build/kernel-clang-aarch64.sh

AEGISOS_QEMU_TIMEOUT=10 AEGISOS_QEMU_REQUIRE_BANNER=1 \
  tools/qemu/smoke-aarch64.sh build/aegisos.bin
```

You can also test the ELF image directly:

```bash
AEGISOS_QEMU_TIMEOUT=10 AEGISOS_QEMU_REQUIRE_BANNER=1 \
  tools/qemu/smoke-aarch64.sh build/aegisos.elf
```

Expected proof of entry:

```text
[AegisOS] semihost: _start reached
```

If semihosting appears but UART does not, `_start` is alive and the next issue
is PL011/UART routing or init. If neither appears, inspect `build/qemu-smoke.log`
and `build/qemu-debug.log`.
