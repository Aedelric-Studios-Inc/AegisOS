/* SPDX-License-Identifier: Proprietary */
/* AegisOS v58 native aegisd lifecycle proof.
 *
 * PID 3 publishes its health endpoint and deliberately exits with status 42.
 * The native service-manager reaps it, releases its runtime resources, and
 * respawns this same ELF as PID 4.  PID 4 publishes health and remains alive.
 */

.section .text.entry, "ax"
.global _start
.type _start, %function

.equ SYS_WRITE,         1
.equ SYS_EXIT,          4
.equ SYS_GETPID,        5
.equ SYS_SEND_MSG,      8
.equ SYS_YIELD,        12
.equ SYS_CHANNEL_OPEN, 18
.equ SYS_SERVICE_READY,19

_start:
    mov     x8, #SYS_GETPID
    svc     #0
    mov     x20, x0

    cmp     x20, #3
    b.eq    banner_initial
    cmp     x20, #4
    b.eq    banner_recovery
    b       fatal

banner_initial:
    mov     x0, #1
    adr     x1, initial_banner
    mov     x2, #(initial_banner_end - initial_banner)
    mov     x8, #SYS_WRITE
    svc     #0
    b       register_service

banner_recovery:
    mov     x0, #1
    adr     x1, recovery_banner
    mov     x2, #(recovery_banner_end - recovery_banner)
    mov     x8, #SYS_WRITE
    svc     #0

register_service:
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

    mov     x0, x19
    adr     x1, health_payload
    mov     x2, #(health_payload_end - health_payload)
    mov     x8, #SYS_SEND_MSG
    svc     #0
    cmp     x0, #0
    b.lt    fatal

    mov     x0, #1
    adr     x1, endpoint_ready
    mov     x2, #(endpoint_ready_end - endpoint_ready)
    mov     x8, #SYS_WRITE
    svc     #0

#ifdef AEGISOS_V58_LIFECYCLE_PROOF
    cmp     x20, #3
    b.ne    daemon_loop

    mov     x0, #1
    adr     x1, deliberate_exit
    mov     x2, #(deliberate_exit_end - deliberate_exit)
    mov     x8, #SYS_WRITE
    svc     #0

    mov     x0, #42
    mov     x8, #SYS_EXIT
    svc     #0
    b       fatal
#endif

daemon_loop:
    mov     x8, #SYS_YIELD
    svc     #0
    b       daemon_loop

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
initial_banner:
    .ascii "[aegisd] native AegisOS userspace online pid=3\n"
initial_banner_end:
recovery_banner:
    .ascii "[aegisd] native AegisOS userspace online pid=4\n"
recovery_banner_end:
endpoint_ready:
    .ascii "[aegisd] native IPC endpoint registered\n"
endpoint_ready_end:
deliberate_exit:
    .ascii "[aegisd] deliberate lifecycle test exit status=42\n"
deliberate_exit_end:
failure:
    .ascii "[aegisd] native daemon lifecycle bootstrap failed\n"
failure_end:
service_name:
    .asciz "aegisd"
health_channel_name:
    .asciz "aegisd-health"
health_payload:
    .ascii "aegisd-ready"
health_payload_end:
