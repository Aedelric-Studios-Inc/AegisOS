# Flashing the AegisOS v55 Release IMG

Build and prove first:

```bash
tools/dev/fix-clock-skew.sh
tools/qemu/build-and-smoke-aarch64.sh
```

Expected release image:

```text
build/images/release-v55/AegisOS-v2.0-v55-release-aarch64.img
```

Verify checksums:

```bash
tools/release/verify-v55-release.sh
```

Manual QEMU boot with the image attached:

```bash
KERNEL="build/aegisos.elf"
IMG="build/images/release-v55/AegisOS-v2.0-v55-release-aarch64.img"
qemu-system-aarch64 \
  -machine virt,secure=off,virtualization=off,gic-version=2 \
  -cpu cortex-a57 \
  -smp 1 \
  -m 1G \
  -nographic \
  -serial mon:stdio \
  -semihosting-config enable=on,target=native \
  -kernel "$KERNEL" \
  -drive file="$IMG",format=raw,if=none,id=aegisflash \
  -device virtio-blk-device,drive=aegisflash \
  -append "console=ttyAMA0,115200"
```

Do not flash real hardware until the target device path is confirmed.
