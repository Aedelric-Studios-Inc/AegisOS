# AegisOS v55.1 P0 Release-Blocker Fixes

This patch closes the critical correctness/security blockers found after the first interactive shell boot.

## Fixed

- v40 image geometry and v41 kernel block model now agree on the 256 MiB layout:
  - `AEGIS_BOOT`: LBA 2048, 65536 sectors
  - `AEGIS_ROOT`: LBA 67584, 327680 sectors
  - `AEGIS_CONFIG`: LBA 395264, 65536 sectors
- Non-smoke interactive runtime prepares the service supervisor, page isolation, network control, Developer Preview, release polish, and release-final state before entering the shell.
- Scheduler yield no longer marks a dead current task READY after `process_exit()` or `kthread_exit()`.
- Joinable kernel threads are marked inactive when they exit, so `kthread_join()` can complete.
- Syscalls validate EL0 pointer ranges and use bounded copyin/copyout helpers for read/write/open/send/recv/spawn/waitpid/service_id.
- `sys_mmap()` no longer creates RWX mappings and rejects WRITE+EXEC requests.
- Capability tokens now advance monotonically; grant/revoke require a valid token and kernel/current-process authority.
- IPC service roundtrip now uses `ipc_send()`/`ipc_recv()` queue delivery rather than a local stack-buffer copy.
- v55 guard accepts both `v55-release-img` and `v55.1-interactive-runtime` release-line VERSION markers.
- Variant images receive per-variant config overlays so Lite/Pro/Bastion/Router artifacts are not byte-identical copies.
- Release image receives a release overlay in the config partition.
- `tools/check-aegisos.sh` now runs the AArch64 kernel link check and host C unit tests by default.

## Still intentionally not claimed

- Native Rust service execution as independent AegisOS EL0 processes.
- Standalone UEFI/firmware boot without `-kernel`.
- Production packet I/O and external network service binding.
