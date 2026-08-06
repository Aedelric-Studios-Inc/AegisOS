# AegisOS v24 - Build Script Restored

v23 accidentally omitted `tools/build/kernel-clang-aarch64.sh` from the packaged archive.
The QEMU smoke wrapper therefore failed before it reached the kernel build.

v24 restores:

- `tools/build/kernel-clang-aarch64.sh`
- executable permissions on the script
- the v23 visible-log/memory/exception proof path

Expected run:

```bash
cd ~/AegisOS
tools/dev/fix-clock-skew.sh
tools/qemu/build-and-smoke-aarch64.sh
```

Expected proof lines:

```text
qemu boot proof: reached qemu_boot_checkpoint_visible_log
qemu boot proof: reached qemu_boot_checkpoint_memory_selftest
qemu boot proof: reached qemu_boot_checkpoint_exception_vectors
QEMU v23 proof accepted: visible logging, memory selftest, and exception vectors reached.
```
