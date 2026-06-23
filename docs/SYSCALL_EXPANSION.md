# AegisOS v42 — Syscall Expansion

v42 expands the syscall surface beyond the first `GETPID`/`EXIT` proof path.

Added syscall IDs:

- `SYS_SPAWN` — request launch of a known userland descriptor by path/name.
- `SYS_WAITPID` — wait/reap interface boundary.
- `SYS_GETPPID` — parent PID query placeholder for the future live process tree.
- `SYS_GETTID` — thread ID query placeholder for future scheduler exposure.
- `SYS_SERVICE_ID` — resolve a userland service/feature name into its catalogue ID.

The implementation is deliberately kernel-safe: calls validate arguments, operate on known descriptors, and preserve the current proven EL0 launch/exit path.
