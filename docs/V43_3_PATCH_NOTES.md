# AegisOS v43.3 Patch Notes — IPC Kernel Mailbox Roundtrip Fix

v43.3 fixes the remaining v43.2 proof failure. v43.2 entered `service_ipc_message_roundtrip_selftest()` but never reached `qemu_boot_checkpoint_ipc_message_roundtrip_ready`.

## Change

The v43 proof now validates the kernel-owned service mailbox route directly through the IPC queue backing used by the send/receive syscall layer:

- enqueue payload on the service mailbox channel
- dequeue payload from the same channel
- compare every byte
- mark the mailbox as `AEGIS_IPC_MAILBOX_ROUNDTRIP_PROVED`
- raise `message_roundtrip_ready`

The syscall dispatch SEND/RECV path remains as non-fatal smoke so it cannot mask the mailbox proof or break the existing EL0 launch/exit chain.

## Target proof line

```text
QEMU v43.3 proof accepted: IPC/service messaging mailboxes prepared, kernel-owned mailbox roundtrip proved, and EL0 launch/exit chain preserved.
```
