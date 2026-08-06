# AegisOS Router Bring-up v28 — Kernel Services Registry + Service Manager

v28 extends the scheduler-managed `aegisbox-init` thread so it performs the next boot responsibility after VFS/initramfs mount:

1. Register built-in kernel services.
2. Prepare the service manager dependency graph.
3. Prove both steps under QEMU trace without starting userland yet.

## New kernel pieces

- `kernel/include/service_manager.h`
- `kernel/core/service_manager.c`

The service manager tracks service ID, name, state, flags, and dependencies.  v28 keeps service execution disabled intentionally; it only proves registration and preparation.

## Built-in kernel services registered

- `vfs`
- `initramfs`
- `devfs`
- `procfs`
- `capability`
- `ipc`
- `security`
- `network`
- `firewall`
- `router`
- `aegisd`

## New QEMU proof checkpoints

- `qemu_boot_checkpoint_kernel_services_registered`
- `qemu_boot_checkpoint_service_manager_prepared`

Expected acceptance line:

```text
QEMU v28 proof accepted: kernel services registered and service manager prepared; userland handoff intentionally deferred.
```

## Deferred to v29+

- Starting services
- Userland handoff
- ELF/user process loader
- `aegisd` process/runtime
