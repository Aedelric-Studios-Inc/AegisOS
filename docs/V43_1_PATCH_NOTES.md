# AegisOS v2.0 v43.1 — IPC Roundtrip Proof Fix

v43.1 patches the IPC/service messaging milestone without advancing to v44.

The v43 QEMU trace showed that `service_ipc_prepare_service_messaging` and
`service_ipc_message_roundtrip_selftest` were reached, but the completion
checkpoint `qemu_boot_checkpoint_ipc_message_roundtrip_ready` was not proved.
That meant the roundtrip proof could stall or fail before the EL0 launch chain.

## Fix

- Reset the low-level IPC queue layer from `service_ipc_init()`.
- Make `ipc_recv()` bounded/non-blocking in QEMU smoke builds.
- Keep the roundtrip proof routed through `SYS_SEND_MSG` and `SYS_RECV_MSG`.
- Preserve the existing v42 EL0 launch, syscall return, process exit, and
  post-exit scheduler handoff proof chain.

## Expected proof line

```text
QEMU v43.1 proof accepted: IPC/service messaging mailboxes prepared, bounded SYS_SEND_MSG/SYS_RECV_MSG roundtrip proved, and EL0 launch/exit chain preserved.
```
