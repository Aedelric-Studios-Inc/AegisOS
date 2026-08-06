# AegisOS Router Bring-up v21 - Packaging Fix

v21 fixes the v20 package omission where `tools/build/kernel-clang-aarch64.sh`
was not included in the archive. The QEMU build wrapper expects this script at:

```text
tools/build/kernel-clang-aarch64.sh
```

The boot-phase changes from v20 remain intact:

- `_start` preserves QEMU's `x0` DTB/FDT pointer.
- `_start` passes `dtb_ptr` into `kernel_main(dtb_ptr)`.
- `kernel_main` enters ordered boot phases.
- QEMU proof scripts can verify entry and boot-phase symbol presence.

Run:

```bash
tools/dev/fix-clock-skew.sh
tools/qemu/build-and-smoke-aarch64.sh
```
