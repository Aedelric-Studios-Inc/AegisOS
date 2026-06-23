# AegisOS v27 — VFS/initramfs bootstrap from first init thread

v27 extends the v26 scheduler-managed init-thread proof by making
`aegisbox-init` perform the first filesystem bring-up work.

## What is proved

The QEMU trace proof now requires these scheduler/init-thread checkpoints:

- `qemu_boot_checkpoint_first_init_thread`
- `qemu_boot_checkpoint_vfs_init`
- `qemu_boot_checkpoint_initramfs_mount`
- `qemu_boot_checkpoint_vfs_mounts`

## Boot-thread sequence

```text
kernel_main
  -> creates idle task
  -> creates aegisbox-init task
  -> scheduler_run()
  -> context_switch()
  -> aegisbox_init_task()
       -> vfs_init()
       -> initramfs_mount_empty_root() at /
       -> vfs_mount_bootstrap_pseudo("/dev", "devfs")
       -> vfs_mount_bootstrap_pseudo("/proc", "procfs")
       -> vfs_bootstrap_selftest()
```

This is not yet a full userspace loader. It is the kernel-init filesystem spine:
root mount, device pseudo-filesystem mount, process pseudo-filesystem mount,
and open/resolve selftests under scheduler-managed execution.

## Expected success line

```text
QEMU v27 proof accepted: init thread mounted VFS/initramfs bootstrap root, /dev, and /proc.
```
