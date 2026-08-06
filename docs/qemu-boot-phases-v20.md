# AegisOS Router Bring-up v20 — Boot Phase Wiring

v19 proved that QEMU's `-kernel build/aegisos.bin` path reaches the AegisOS
ARM64 entry point at `0x40080000`.

v20 wires the next layer:

1. `_start` preserves the QEMU/FDT pointer from `x0`.
2. `_start` passes that pointer to `kernel_main(dtb_ptr)`.
3. `kernel_main` initialises a named boot phase coordinator.
4. early platform, DTB, memory, core, device, service, security, network, and
   init phases are logged in order.
5. QEMU smoke builds no longer stop inside assembly before reaching C.
6. `tools/qemu/prove-boot-phases-aarch64.sh` proves that QEMU reaches:
   - `_start`
   - `_aegisos_reset_entry`
   - `kernel_main`
   - `boot_phase_enter`

This still is not full userspace boot.  The next milestones are visible UART
output, exception-vector proof, scheduler proof, VFS/initramfs, then service
manager/aegisd/router service wiring.
