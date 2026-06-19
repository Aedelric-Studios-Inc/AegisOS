# AegisOS Syscall ABI

## Calling Convention (AArch64)

System calls are invoked via the `svc #0` instruction.

| Register | Role                                      |
|----------|-------------------------------------------|
| x8       | Syscall number                            |
| x0–x5    | Arguments (up to 6)                       |
| x0       | Return value (or negative errno on error) |

## Syscall Table

| Number | Name             | Description                        |
|--------|------------------|------------------------------------|
| 0      | `sys_read`       | Read from file descriptor          |
| 1      | `sys_write`      | Write to file descriptor           |
| 2      | `sys_open`       | Open file                          |
| 3      | `sys_close`      | Close file descriptor              |
| 4      | `sys_exit`       | Terminate process                  |
| 5      | `sys_getpid`     | Get process ID                     |
| 6      | `sys_mmap`       | Map memory                         |
| 7      | `sys_munmap`     | Unmap memory                       |
| 8      | `sys_send_msg`   | IPC message send                   |
| 9      | `sys_recv_msg`   | IPC message receive                |
| 10     | `sys_cap_grant`  | Grant capability to another task   |
| 11     | `sys_cap_revoke` | Revoke capability                  |
| 12     | `sys_yield`      | Yield CPU to scheduler             |

## Entry Point

The syscall entry stub lives in `boot/arm64/syscall_entry.S`.
It saves registers, dispatches through `kernel/core/syscall_table.c`, and restores state on return.
