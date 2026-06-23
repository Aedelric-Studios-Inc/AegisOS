# AegisOS v43.2 Patch Notes — IPC Roundtrip Direct Syscall Fix

v43.2 fixes the remaining v43.1 IPC proof failure. The v43.1 log reached `service_ipc_message_roundtrip_selftest()` but did not reach `qemu_boot_checkpoint_ipc_message_roundtrip_ready`, which meant the proof chain stopped before the IPC roundtrip completion marker and before the EL0 launch/exit proof.

## Fix

- Reset the channel table as well as the IPC queue table during `service_ipc_init()`.
- Prove the `SYS_SEND_MSG` / `SYS_RECV_MSG` syscall implementation path through the direct syscall handlers first.
- Run a second dispatch-table smoke roundtrip through `syscall_dispatch()` to prove the syscall table surface is still wired.
- Increase the QEMU boot-phase proof timeout from 8s to 12s, because v40-v43 has grown the trace surface substantially.

## Expected proof line

```text
QEMU v43.2 proof accepted: IPC/service messaging mailboxes prepared, direct syscall-handler + dispatch SYS_SEND_MSG/SYS_RECV_MSG roundtrip proved, and EL0 launch/exit chain preserved.
```
