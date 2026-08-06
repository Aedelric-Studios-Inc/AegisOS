# AegisOS v57 — Native Supervisor Chain

## Scope

v57 promotes the first service chain from catalogue entries into separate
AegisOS-native EL0 processes:

```text
PID 1  /sbin/aegis-init
  └─ PID 2  /sbin/service-manager
       └─ PID 3  /sbin/aegisd
```

All three bootstrap images are freestanding AArch64 ELF files embedded in the
AegisOS initramfs. They execute through the AegisOS ELF loader, process table,
cooperative scheduler, exception path, syscall table, service supervisor, and
IPC queues. No Linux host process, systemd unit, host Unix socket, or host
network service participates in this proof.

## Implemented in v57

- A bounded native ELF runtime pool with distinct executable backing and EL0
  stacks for each process.
- A real `SYS_SPAWN` path that loads a VFS ELF, creates a process record and
  scheduler task, and returns the child PID.
- Persistent PID 1 rather than the v56 exit-after-proof behaviour.
- Native service-manager and aegisd bootstrap ELFs.
- Native service state registration through `SYS_SERVICE_READY`.
- Named AegisOS IPC channels through `SYS_CHANNEL_OPEN`.
- A service-manager/aegisd health-message roundtrip using `SYS_SEND_MSG` and
  `SYS_RECV_MSG`.
- Live process and service-supervisor state in the interactive console.

## Required QEMU proof

Run:

```bash
AEGISOS_QEMU_TIMEOUT=30 tools/qemu/prove-native-supervisor-aarch64.sh
```

The proof is accepted only when the guest serial log contains the complete
PID 1 -> PID 2 -> PID 3 launch chain, both native-running supervisor state
transitions, the aegisd IPC endpoint line, and the service-manager health-check
completion line.

## Deliberate boundary

The v57 service-manager and aegisd images are small freestanding assembly
bootstrap processes. They prove the native process/supervision/IPC mechanism;
they are not the complete Rust service-manager and aegisd applications.
Promoting the production Rust applications requires an AegisOS Rust target and
userspace runtime/standard-library port.

Timer IRQ accounting remains enabled, but timer-IRQ task switching remains
disabled. Scheduling is cooperative through `SYS_YIELD` until exception-frame-
aware preemption is implemented and validated.
