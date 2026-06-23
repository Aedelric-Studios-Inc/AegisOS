# AegisOS-v2.0 v42 Release Notes

## Milestone

v42 combines:

- Real multi-process launch path.
- Syscall expansion.

## Proof target

```text
QEMU v42 proof accepted: real multi-process launch path prepared and expanded syscall surface proved.
```

## Relationship to v41

v41 gave the kernel a block/storage model and cleaned process table invariants. v42 uses that cleaner process foundation to promote the userland descriptors into a launchable service set.

## Next

v43: IPC/service messaging.
