# AegisOS v10 QEMU Entry Diagnosis

The v9 logs showed a successful QEMU launch but no guest output. v10 changes the smoke path in two ways:

1. The QEMU smoke build now prints the PL011 UART banner before attempting semihosting. This prevents a semihosting trap from blocking UART proof.
2. The all-in-one smoke wrapper now tries three load paths: generic ELF loader, `-kernel` ELF, and `-kernel` raw ARM64 Image.

Run:

```bash
tools/dev/fix-clock-skew.sh
tools/qemu/sanity-aarch64.sh
tools/qemu/build-and-smoke-aarch64.sh
```

If the sanity test passes but AegisOS fails, the problem is inside AegisOS entry/kernel setup. If sanity fails too, the problem is host QEMU serial/semihosting configuration.

If all modes fail, upload:

```text
build/qemu-sanity/smoke.log
build/qemu-sanity/debug.log
build/qemu-smoke-loader-elf.log
build/qemu-debug-loader-elf.log
build/qemu-smoke-kernel-elf.log
build/qemu-debug-kernel-elf.log
build/qemu-smoke-kernel-bin.log
build/qemu-debug-kernel-bin.log
```
