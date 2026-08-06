# AegisOS router bring-up v29 — userland handoff foundation

v29 connects the scheduler-managed `aegisbox-init` thread to the first userland handoff layer.

This stage intentionally prepares the handoff contract rather than launching a real EL0 ELF image.  The next milestone should implement the actual EL0 transition, process address-space setup, and ELF loader path.

## What v29 proves

The QEMU trace proof now verifies that AegisOS reaches these additional checkpoints:

- `qemu_boot_checkpoint_userland_catalogue`
- `qemu_boot_checkpoint_userland_service_links`
- `qemu_boot_checkpoint_userland_handoff_ready`

The handoff layer confirms:

- VFS/bootstrap root is mounted.
- `/dev` and `/proc` are mounted.
- Kernel services are registered and prepared.
- Built-in userland feature records are registered.
- Feature records are bound to prepared kernel services.
- The syscall dispatch surface is present.
- All feature records are marked `handoff-ready`.

## Built-in feature catalogue

v29 registers and binds these feature records:

- `aegis-init` → `aegisd`
- `aegisd` → `aegisd`
- `securityd` → `security`
- `ipcd` → `ipc`
- `capability-broker` → `capability`
- `netd` → `network`
- `firewalld` → `firewall`
- `routerd` → `router`
- `devmgr` → `devfs`
- `procd` → `procfs`
- `rustmyadmin` → `aegisd`
- `phoenix-feature-bridge` → `router`

This connects the current AegisBox/Phoenix feature map without pretending the EL0 runtime is finished.

## Expected proof line

```text
QEMU v29 proof accepted: userland feature catalogue, service links, and handoff contract prepared; EL0 launch intentionally deferred.
```
