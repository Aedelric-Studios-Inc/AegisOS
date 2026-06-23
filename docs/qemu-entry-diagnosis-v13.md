# AegisOS v13 QEMU Harness Fix

The v12 logs proved that QEMU 11.0.1 exists and that both sanity ELF and
ARM64 Image binaries build, but every mode timed out with empty output.

V13 changes the QEMU harness rather than the kernel: it adds a true bare
firmware sanity payload, tries `-bios` first, and fixes generic-loader PC setup
by splitting file loading from CPU0 PC assignment.

Run:

```sh
cd ~/AegisOS
tools/dev/fix-clock-skew.sh
tools/qemu/qemu-env-report.sh
tools/qemu/sanity-aarch64.sh
tools/qemu/build-and-smoke-aarch64.sh
```

Interpretation:

- If `bios-bare` passes, QEMU serial/semihosting works and the remaining issue
  is loader/Image entry.
- If all sanity modes fail, inspect the new sanity logs; this is still below
  AegisOS itself.
- If sanity passes but AegisOS fails, focus on AegisOS early entry, stack, BSS,
  and UART path.
