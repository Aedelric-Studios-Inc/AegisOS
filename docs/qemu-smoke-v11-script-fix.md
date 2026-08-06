# AegisOS v11 QEMU Smoke Script Fix

## What changed

The v10 QEMU smoke runner failed before it could report useful QEMU output on some shells because it used:

```bash
printf '--- qemu output ---\n'
```

Bash `printf` can parse a format string beginning with `--` as an option, producing:

```text
printf: --: invalid option
```

The smoke runner now uses:

```bash
printf '%s\n' '--- qemu output ---'
```

This keeps the same log output while avoiding option parsing.

## Why this matters

The previous failure did not prove an AegisOS boot failure. It stopped in the host-side test script before QEMU logs could be trusted. v11 restores the QEMU diagnostic path so the next run can distinguish between:

- host QEMU serial/semihosting failure,
- QEMU image-loader failure,
- AegisOS early-entry failure,
- UART output failure.

## Recommended run

```bash
cd ~/AegisOS
tools/dev/fix-clock-skew.sh
tools/qemu/sanity-aarch64.sh
tools/qemu/build-and-smoke-aarch64.sh
```

If it still fails, upload the generated `build/qemu-smoke-*.log` and `build/qemu-debug-*.log` files.
