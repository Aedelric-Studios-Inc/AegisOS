# AegisOS v56 native PID 1 bootstrap

This tranche replaces the fake built-in `/sbin/aegis-init` ELF payload with a
real freestanding AArch64 executable and launches it as PID 1 at EL0 inside the
AegisOS guest.

## Native boundary implemented

The normal non-smoke boot path now:

1. builds `userland/native/aegis-init.S` as an ELF64/AArch64 executable;
2. embeds that ELF into the AegisOS initramfs at `/sbin/aegis-init`;
3. opens and validates the ELF through the AegisOS VFS and ELF loader;
4. copies its `PT_LOAD` segment into AegisOS-owned runtime memory;
5. allocates an AegisOS-owned EL0 stack;
6. binds the current scheduler task to process PID 1;
7. enters the loaded ELF at EL0;
8. handles `write`, `getpid`, service-registry lookup and `exit` through the
   AegisOS syscall/exception path;
9. switches to the AegisOS interactive console after PID 1 exits.

No Linux process, systemd unit, host socket, NetworkManager operation or host
firewall command is involved in this native PID 1 execution path.

## Proof command

```bash
cd ~/AegisOS
tools/qemu/prove-native-pid1-aarch64.sh
```

The proof requires these guest lines:

```text
[AegisOS:native] launching /sbin/aegis-init pid=1
[aegis-init] native EL0 PID 1 online under AegisOS
[aegis-init] AegisOS process binding confirmed: pid=1
[aegis-init] service-manager and aegisd kernel registry links confirmed
AegisOS v2.0 v56 native-userland runtime
```

## Current boundary

This tranche makes `aegis-init` a real native process. `service-manager` and
`aegisd` are visible to PID 1 through the AegisOS service registry, but they are
not yet separate long-running EL0 processes. The next native tranche must add
multi-image ELF slots, per-process EL0 contexts, a spawn/wait lifecycle and
native IPC endpoints before the Rust services can be promoted one by one.
