# AegisOS QEMU Diagnosis v12

v11 proved the host-side script bug was fixed, but the standalone sanity image still did not print a banner.  v12 changes the VM harness so the sanity image and AegisOS kernel are tested through multiple boot paths:

1. `loader-elf` — QEMU generic loader with ELF entry point and `-bios none`.
2. `loader-raw` — QEMU generic loader with raw binary loaded at `0x40080000` and CPU0 PC set to that address.
3. `kernel-bin` — QEMU `-kernel` with ARM64 Image/raw binary.
4. `kernel-elf` — QEMU `-kernel` with ELF.

v12 also changes automated serial to `-monitor none -serial stdio` by default, because it is simpler than `-serial mon:stdio` for log capture.

Run:

```bash
cd ~/AegisOS
tools/dev/fix-clock-skew.sh
tools/qemu/qemu-env-report.sh
tools/qemu/sanity-aarch64.sh
tools/qemu/build-and-smoke-aarch64.sh
```

If all modes fail, upload the listed `build/qemu-*` logs.  The sanity logs are especially important: if the tiny sanity payload cannot print, the problem is QEMU launch/serial/loader behaviour rather than AegisOS itself.
