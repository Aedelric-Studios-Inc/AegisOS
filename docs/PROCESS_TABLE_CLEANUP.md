# Process Table Cleanup

AegisOS v41 prepares the process table for v42's real multi-process launch path.

The kernel now exposes process table stats and a cleanup selftest that proves:

- empty process slots are normalised,
- live PID values are non-zero,
- live PID values are unique,
- state counters match used slots,
- process table cleanup completed before launching the first userspace proof process.

This keeps v42 focused on actually launching multiple EL0 processes instead of cleaning up process accounting first.
