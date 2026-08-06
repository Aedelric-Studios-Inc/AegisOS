# AegisOS v2.0 v43 Release Notes

## Milestone

```text
AegisOS-v2.0-v43-ipc-service-messaging
```

V43 adds IPC/service messaging on top of the v42 multi-process launch and syscall-expansion milestone.

## Added

- `kernel/include/ipc_service.h`
- `kernel/ipc/service_ipc.c`
- Service mailbox/channel registry
- Kernel-safe service message roundtrip proof
- QEMU checkpoints for IPC/service messaging
- Updated v43 milestone guard

## Proved

```text
service mailbox channels are created
service routes are registered
SYS_SEND_MSG can enqueue a payload
SYS_RECV_MSG can receive the same payload
message roundtrip validates byte-for-byte
```

## Expected final line

```text
QEMU v43 proof accepted: IPC/service messaging mailboxes prepared and kernel-safe message roundtrip proved.
```

## Next milestone

V44 combines:

```text
service supervisor
fault handling / page-fault proof
```
