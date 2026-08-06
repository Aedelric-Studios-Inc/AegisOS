/* SPDX-License-Identifier: Proprietary */
/* AegisOS native PID 1 bootstrap.
 *
 * This image is linked as a freestanding AArch64 ELF and embedded in the
 * AegisOS initramfs.  It executes at EL0 under the AegisOS kernel and uses
 * only the AegisOS syscall ABI.  It deliberately avoids Linux, libc, systemd,
 * and host services.
 */

.section .text.entry, "ax"
.global _start
.type _start, %function

.equ SYS_WRITE,      1
.equ SYS_EXIT,       4
.equ SYS_GETPID,     5
.equ SYS_YIELD,      12
.equ SYS_SPAWN,      13
.equ SYS_SERVICE_ID, 17
.equ SYS_SERVICE_READY, 19

_start:
    /* Prove that this is a real AegisOS userspace process. */
    mov     x0, #1
    adr     x1, banner
    mov     x2, #(banner_end - banner)
    mov     x8, #SYS_WRITE
    svc     #0

    /* PID 1 must be bound to the current AegisOS scheduler task. */
    mov     x8, #SYS_GETPID
    svc     #0
    cmp     x0, #1
    b.ne    boot_failed

    mov     x0, #1
    adr     x1, pid_ok
    mov     x2, #(pid_ok_end - pid_ok)
    mov     x8, #SYS_WRITE
    svc     #0

    /* Resolve the first native service dependencies through the kernel's
     * AegisOS userland/service registry.  Positive IDs prove the catalogue is
     * wired to this EL0 process through the AegisOS syscall ABI.
     */
    adr     x0, service_manager_name
    mov     x8, #SYS_SERVICE_ID
    svc     #0
    cmp     x0, #0
    b.le    boot_failed

    adr     x0, aegisd_name
    mov     x8, #SYS_SERVICE_ID
    svc     #0
    cmp     x0, #0
    b.le    boot_failed

    mov     x0, #1
    adr     x1, registry_ok
    mov     x2, #(registry_ok_end - registry_ok)
    mov     x8, #SYS_WRITE
    svc     #0

    adr     x0, init_name
    mov     x8, #SYS_SERVICE_READY
    svc     #0
    cmp     x0, #0
    b.lt    boot_failed

    adr     x0, service_manager_path
    mov     x1, #0
    mov     x8, #SYS_SPAWN
    svc     #0
    cmp     x0, #2
    b.ne    boot_failed

    mov     x0, #1
    adr     x1, spawned_manager
    mov     x2, #(spawned_manager_end - spawned_manager)
    mov     x8, #SYS_WRITE
    svc     #0

    /* PID 1 remains alive and cooperatively schedules the native supervisor
     * chain.  Exit/restart policy is added after the persistent wait/reap path.
     */
1:
    mov     x8, #SYS_YIELD
    svc     #0
    b       1b

boot_failed:
    mov     x0, #2
    adr     x1, failure
    mov     x2, #(failure_end - failure)
    mov     x8, #SYS_WRITE
    svc     #0

    mov     x0, #1
    mov     x8, #SYS_EXIT
    svc     #0
2:
    wfe
    b       2b

.size _start, . - _start

.section .rodata, "a"
banner:
    .ascii "[aegis-init] native EL0 PID 1 online under AegisOS\n"
banner_end:
pid_ok:
    .ascii "[aegis-init] AegisOS process binding confirmed: pid=1\n"
pid_ok_end:
registry_ok:
    .ascii "[aegis-init] service-manager and aegisd kernel registry links confirmed\n"
registry_ok_end:
spawned_manager:
    .ascii "[aegis-init] spawned service-manager pid=2\n"
spawned_manager_end:
failure:
    .ascii "[aegis-init] native bootstrap failed\n"
failure_end:
init_name:
    .asciz "aegis-init"
service_manager_name:
    .asciz "service-manager"
aegisd_name:
    .asciz "aegisd"
service_manager_path:
    .asciz "/sbin/service-manager"
