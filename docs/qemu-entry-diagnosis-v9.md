# AegisOS v9 QEMU Entry Diagnosis

The v8 strict smoke test could launch QEMU but produced an empty QEMU output
log. That narrowed the issue to the QEMU loader/entry path rather than the Rust,
C, assembly, or linker checks.

## What changed in v9

- `boot/arm64/start.S` now begins with an AArch64 Image-style header.
- The first instruction still branches into the real AegisOS reset entry, so raw
  loaders remain supported.
- The header includes the `ARM64_IMAGE_MAGIC` word expected by AArch64 Image
  loaders.
- `tools/qemu/build-and-smoke-aarch64.sh` now builds with
  `AEGISOS_QEMU_SMOKE=1` automatically and then runs strict QEMU.
- `tools/qemu/smoke-aarch64.sh` now warns if the strict target does not contain
  the expected UART or semihosting banner strings.

## Recommended test

```bash
cd AegisOS
tools/dev/fix-clock-skew.sh
tools/qemu/build-and-smoke-aarch64.sh
```

Expected proof:

```text
[AegisOS] semihost: _start reached
```

or:

```text
[AegisOS] early boot: _start reached
```

Either result proves the CPU reached the AegisOS reset path.

## If it still fails with empty output

Upload both:

```text
build/qemu-smoke.log
build/qemu-debug.log
```

`qemu-smoke.log` shows command/output. `qemu-debug.log` can show whether QEMU
hit an unsupported instruction, unmapped device, or guest exception before the
banner printed.
