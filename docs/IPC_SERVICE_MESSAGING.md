# AegisOS v43 IPC / Service Messaging

V43 adds the first kernel-proved IPC/service messaging layer on top of the v42 multi-process and syscall surface.

V42 proved that the early userland service descriptors are launchable and that the syscall table has process/service control calls. V43 proves those services can be connected through mailbox channels and that the low-level message queue path can move a payload safely. The EL0 syscall wrappers copy through the same queue boundary after user pointer validation; the deterministic smoke proof keeps the payload kernel-owned so it does not depend on unsafe EL0 pointers.

## Scope

V43 provides:

```text
service mailbox registry
kernel-owned IPC channels
queue creation for service routes
kernel IPC queue roundtrip proof with SYS_SEND_MSG/SYS_RECV_MSG wrappers wired
QEMU checkpoints for service messaging readiness
```

The current route proof covers the early AegisBox service chain:

```text
aegis-init  -> aegisd
aegisd      -> routerd
routerd     -> rustmyadmin
rustmyadmin -> aegisd
```

## New kernel files

```text
kernel/include/ipc_service.h
kernel/ipc/service_ipc.c
```

The v43 layer sits above the existing low-level IPC primitives:

```text
kernel/ipc/channel.c
kernel/ipc/message.c
```

## Proof sequence

The v43 init path runs after v42 has proved real multi-process launch and syscall expansion:

```text
v42 multi-process launch ready
→ v42 syscall expansion ready
→ service_ipc_init
→ service_ipc_prepare_service_messaging
→ mailbox/channel registry selftest
→ service_ipc_message_roundtrip_selftest
→ ipc_send payload queued on the registered service channel
→ ipc_recv payload received from the registered service channel
→ byte-for-byte payload validation
```

## QEMU proof markers

```text
service_ipc_prepare_service_messaging
qemu_boot_checkpoint_ipc_service_messaging_ready
service_ipc_message_roundtrip_selftest
qemu_boot_checkpoint_ipc_message_roundtrip_ready
```

Expected proof line:

```text
QEMU v43 proof accepted: IPC/service messaging mailboxes prepared and kernel-safe message roundtrip proved.
```

## Boundary

V43 is still a deterministic kernel bring-up proof. It does not yet mean independent live EL0 services are scheduling and passing production messages concurrently. That comes after v44/v45 hardening work.

What v43 means is that AegisOS now has the first service communication contract between the early process/service descriptors.
