# AegisOS Router Bring-up v18 — QEMU Kernel-Path Proof

## What changed

The v17 HMP probe finally proved that QEMU is executing guest code. The key
path is now the QEMU `-kernel` ARM64 Image path, not generic-loader.

Observed host behaviour:

- QEMU starts an ARM64 stub at `0x40000000`.
- The stub passes the DTB pointer in `x0`; observed `x0 = 0x48000000`.
- The stub branches into the loaded image at `0x40080000`.
- The payload executes at `0x40080000`.
- BIOS-mode semihosting executes `SYS_WRITE0` and `SYS_EXIT`.
- Generic-loader at `0x40080000` can overlap QEMU's generated DTB region
  `0x40000000..0x40100000`, so it is no longer used as the default smoke path.

## v18 policy

Default smoke path:

```bash
tools/qemu/build-and-smoke-aarch64.sh
```

uses:

```bash
qemu-system-aarch64 ... -kernel build/aegisos.bin
```

The smoke harness now accepts either visible stdout banners or trace proof of
entry at `0x40080000` / semihost execution. Some QEMU builds handle semihosting
without emitting the write string to stdout, so trace proof is a valid smoke
result for bring-up.

## Optional diagnostics

Generic-loader probes are disabled by default because of the DTB overlap. To
run them deliberately:

```bash
AEGISOS_QEMU_PROBE_LOADERS=1 tools/qemu/probe-entry-aarch64.sh
AEGISOS_QEMU_TRY_UNSAFE_LOADERS=1 tools/qemu/sanity-aarch64.sh
```

## QEMU smoke build behaviour

When `AEGISOS_QEMU_SMOKE=1` is used, the early assembly marker now performs:

1. PL011 UART attempt.
2. Semihost `SYS_WRITE0`.
3. Semihost `SYS_EXIT`.

That keeps early-entry smoke focused on the boot entry itself instead of later
kernel subsystems.
