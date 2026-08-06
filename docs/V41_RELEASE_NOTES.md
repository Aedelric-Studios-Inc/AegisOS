# AegisOS v41 — Block/Storage Abstraction + Process Table Cleanup

v41 compresses two foundation milestones into one release:

- Basic block/storage abstraction for the v40 flash image layout.
- Process table cleanup and invariant proof before the real multi-process launch path in v42.

## Block/storage abstraction

The kernel now has a first-class block registry that represents the v40 flash image as:

- `aegis-flash0`
- `AEGIS_BOOT`
- `AEGIS_ROOT`
- `AEGIS_CONFIG`

This is not a final hardware block driver yet. It is the kernel contract that storage drivers, configfs, rootfs, and the image builder will target.

## Process table cleanup

v41 also prepares the process subsystem for real multi-process launching by proving:

- empty slots are normalised,
- PID allocation invariants are explicit,
- duplicate live PIDs are rejected,
- table stats are available for process supervision,
- the process table can be validated before v42 launches multiple EL0 services.

## New proof markers

```text
qemu_boot_checkpoint_block_storage_abstraction_ready
qemu_boot_checkpoint_process_table_cleanup_ready
```

## Expected final proof line

```text
QEMU v41 proof accepted: block/storage abstraction registered the v40 flash layout and process table cleanup invariants held.
```
