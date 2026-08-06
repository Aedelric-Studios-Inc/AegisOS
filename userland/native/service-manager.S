/* SPDX-License-Identifier: Proprietary */
/* AegisOS v58 native service-manager lifecycle proof.
 *
 * The manager starts aegisd as PID 3, validates its health message, waits for
 * its deliberate exit status 42, reaps it, and restarts the same native ELF as
 * PID 4.  The foreground shell is released only after PID 4 passes health.
 */

.section .text.entry, "ax"
.global _start
.type _start, %function

.equ SYS_WRITE,         1
.equ SYS_GETPID,        5
.equ SYS_RECV_MSG,      9
.equ SYS_YIELD,        12
.equ SYS_SPAWN,        13
.equ SYS_WAITPID,      14
.equ SYS_CHANNEL_OPEN, 18
.equ SYS_SERVICE_READY,19
.equ SYS_CONSOLE_READY,20

.equ AEGIS_EAGAIN_MAG,   8

_start:
    sub     sp, sp, #96

    mov     x0, #1
    adr     x1, banner
    mov     x2, #(banner_end - banner)
    mov     x8, #SYS_WRITE
    svc     #0

    mov     x8, #SYS_GETPID
    svc     #0
    cmp     x0, #2
    b.ne    fatal

    adr     x0, service_name
    mov     x8, #SYS_SERVICE_READY
    svc     #0
    cmp     x0, #0
    b.lt    fatal

    adr     x0, health_channel_name
    mov     x8, #SYS_CHANNEL_OPEN
    svc     #0
    cmp     x0, #0
    b.le    fatal
    mov     x19, x0

    adr     x0, aegisd_path
    mov     x1, #0
    mov     x8, #SYS_SPAWN
    svc     #0
    cmp     x0, #3
    b.ne    fatal
    mov     x20, x0

    mov     x0, #1
    adr     x1, spawned_initial
    mov     x2, #(spawned_initial_end - spawned_initial)
    mov     x8, #SYS_WRITE
    svc     #0

wait_initial_health:
    mov     x0, x19
    mov     x1, sp
    mov     x2, #64
    mov     x8, #SYS_RECV_MSG
    svc     #0
    cmp     x0, #(health_payload_end - health_payload)
    b.ne    retry_initial_health

    mov     x3, #0
    adr     x4, health_payload
validate_initial_health:
    cmp     x3, #(health_payload_end - health_payload)
    b.eq    initial_health_ok
    ldrb    w5, [sp, x3]
    ldrb    w6, [x4, x3]
    cmp     w5, w6
    b.ne    retry_initial_health
    add     x3, x3, #1
    b       validate_initial_health

retry_initial_health:
    mov     x8, #SYS_YIELD
    svc     #0
    b       wait_initial_health

initial_health_ok:
    mov     x0, #1
    adr     x1, initial_health_passed
    mov     x2, #(initial_health_passed_end - initial_health_passed)
    mov     x8, #SYS_WRITE
    svc     #0

#ifdef AEGISOS_V58_LIFECYCLE_PROOF
wait_initial_exit:
    mov     x0, x20
    add     x1, sp, #80
    mov     x8, #SYS_WAITPID
    svc     #0
    /* waitpid returns the reaped child PID on success and AEGIS_EAGAIN
     * while the child is still running.  The previous proof expected the old
     * zero-on-success convention and therefore entered fatal after PID 3 had
     * already been reaped successfully.
     */
    cmp     x0, x20
    b.eq    initial_reaped
    cmn     x0, #AEGIS_EAGAIN_MAG
    b.ne    fatal
    mov     x8, #SYS_YIELD
    svc     #0
    b       wait_initial_exit

initial_reaped:
    ldr     w3, [sp, #80]
    cmp     w3, #42
    b.ne    fatal

    mov     x0, #1
    adr     x1, reaped_initial
    mov     x2, #(reaped_initial_end - reaped_initial)
    mov     x8, #SYS_WRITE
    svc     #0

    mov     x0, #1
    adr     x1, restarting
    mov     x2, #(restarting_end - restarting)
    mov     x8, #SYS_WRITE
    svc     #0

restart_spawn:
    /* The supervisor applies a bounded restart backoff.  Retry only when the
     * kernel reports AEGIS_EAGAIN; any other error is a real lifecycle fault.
     */
    adr     x0, aegisd_path
    mov     x1, #0
    mov     x8, #SYS_SPAWN
    svc     #0
    cmp     x0, #4
    b.eq    restart_spawned
    cmn     x0, #AEGIS_EAGAIN_MAG
    b.ne    fatal
    mov     x8, #SYS_YIELD
    svc     #0
    b       restart_spawn

restart_spawned:
    mov     x20, x0

    mov     x0, #1
    adr     x1, spawned_recovery
    mov     x2, #(spawned_recovery_end - spawned_recovery)
    mov     x8, #SYS_WRITE
    svc     #0

wait_recovery_health:
    mov     x0, x19
    mov     x1, sp
    mov     x2, #64
    mov     x8, #SYS_RECV_MSG
    svc     #0
    cmp     x0, #(health_payload_end - health_payload)
    b.ne    retry_recovery_health

    mov     x3, #0
    adr     x4, health_payload
validate_recovery_health:
    cmp     x3, #(health_payload_end - health_payload)
    b.eq    recovery_health_ok
    ldrb    w5, [sp, x3]
    ldrb    w6, [x4, x3]
    cmp     w5, w6
    b.ne    retry_recovery_health
    add     x3, x3, #1
    b       validate_recovery_health

retry_recovery_health:
    mov     x8, #SYS_YIELD
    svc     #0
    b       wait_recovery_health

recovery_health_ok:
    mov     x0, #1
    adr     x1, recovery_health_passed
    mov     x2, #(recovery_health_passed_end - recovery_health_passed)
    mov     x8, #SYS_WRITE
    svc     #0

    mov     x8, #SYS_CONSOLE_READY
    svc     #0
    cmp     x0, #0
    b.lt    fatal

#else
    mov     x8, #SYS_CONSOLE_READY
    svc     #0
    cmp     x0, #0
    b.lt    fatal

#endif
supervisor_loop:
    mov     x8, #SYS_YIELD
    svc     #0
    b       supervisor_loop

fatal:
    mov     x0, #2
    adr     x1, failure
    mov     x2, #(failure_end - failure)
    mov     x8, #SYS_WRITE
    svc     #0
1:
    mov     x8, #SYS_YIELD
    svc     #0
    b       1b

.size _start, . - _start

.section .rodata, "a"
banner:
    .ascii "[service-manager] native AegisOS userspace online pid=2\n"
banner_end:
spawned_initial:
    .ascii "[service-manager] spawned aegisd pid=3\n"
spawned_initial_end:
initial_health_passed:
    .ascii "[service-manager] aegisd initial IPC health check passed\n"
initial_health_passed_end:
reaped_initial:
    .ascii "[service-manager] reaped aegisd pid=3 status=42\n"
reaped_initial_end:
restarting:
    .ascii "[service-manager] restarting aegisd attempt=1\n"
restarting_end:
spawned_recovery:
    .ascii "[service-manager] restarted aegisd pid=4\n"
spawned_recovery_end:
recovery_health_passed:
    .ascii "[service-manager] aegisd recovery IPC health check passed\n"
recovery_health_passed_end:
failure:
    .ascii "[service-manager] native lifecycle recovery failed\n"
failure_end:
service_name:
    .asciz "service-manager"
health_channel_name:
    .asciz "aegisd-health"
aegisd_path:
    .asciz "/sbin/aegisd"
health_payload:
    .ascii "aegisd-ready"
health_payload_end:
