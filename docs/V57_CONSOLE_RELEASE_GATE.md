# AegisOS v57 console release gate

The v57 supervisor processes were healthy, but the interactive console printed
its banner and prompt before PID 2 and PID 3 completed startup. Their later UART
messages overwrote the visible prompt, leaving the VM looking frozen at:

```text
[service-manager] aegisd IPC health check passed
```

This update adds `SYS_CONSOLE_READY` (20). Only the native `service-manager` may
call it, and only while both `service-manager` and `aegisd` are registered as
native-running. `service-manager` calls it after validating the complete
`aegisd-ready` IPC payload. The kernel console task remains schedulable but does
not print or read the UART until that gate opens.

Expected ordering:

```text
[service-manager] aegisd IPC health check passed
[AegisOS:console] supervisor IPC health confirmed; releasing ttyAMA0 shell

AegisOS v2.0 v57 native-supervisor runtime
ttyAMA0 shell online. Type help. Ctrl+A X quits QEMU.

aegis:/#
```

This is a console-startup ordering correction. It does not claim timer-driven
pre-emption or convert the kernel-hosted shell into an EL0 process.
