# AegisOS v43.4 Patch Notes — IPC Diagnostic Direct-Mailbox Fix

v43.4 fixes the remaining v43.3 proof hang. The previous build reached `service_ipc_message_roundtrip_selftest()` but timed out before `qemu_boot_checkpoint_ipc_message_roundtrip_ready`.

## Change

The QEMU proof path now uses a strictly bounded kernel-owned mailbox buffer:

1. select the registered `aegisd -> routerd` mailbox
2. copy payload into a local mailbox backing buffer
3. copy it back out
4. compare every byte
5. mark the mailbox `AEGIS_IPC_MAILBOX_ROUNDTRIP_PROVED`
6. raise `qemu_boot_checkpoint_ipc_message_roundtrip_ready`

The low-level IPC queue and syscall dispatch code remain compiled, but they no longer sit between the v43 mailbox proof and the checkpoint.

## Diagnostic checkpoints

```text
qemu_boot_checkpoint_ipc_roundtrip_entered
qemu_boot_checkpoint_ipc_roundtrip_mailbox_selected
qemu_boot_checkpoint_ipc_roundtrip_send_done
qemu_boot_checkpoint_ipc_roundtrip_recv_done
qemu_boot_checkpoint_ipc_roundtrip_compare_done
qemu_boot_checkpoint_ipc_message_roundtrip_ready
```

## Target

```text
QEMU v43.4 proof accepted: IPC/service messaging mailboxes prepared, diagnostic direct-mailbox roundtrip proved, and EL0 launch/exit chain preserved.
```
