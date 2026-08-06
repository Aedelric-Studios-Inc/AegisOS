# PR1 runtime proof repair

This repair addresses three independently observed failures:

1. Native lifecycle restart stalled after `restarting aegisd attempt=1` because restart backoff used the IRQ-driven software tick counter after the timer proof had disabled that IRQ. Restart policy now uses the architectural monotonic counter in 100 Hz units.
2. The Rust build generated correct ELFs, but unconditional Makefile assembly-header rules rebuilt the same generated headers afterwards. Rust mode now owns those headers exclusively and validates embedded Rust markers.
3. Direct QEMU proofs passed an ELF to `-kernel`; QEMU entered it with `x0=0`, so AegisOS never received the FDT. Proofs now boot the ARM64 raw Image (`aegisos.bin`) and allow QEMU to generate the final DTB containing the attached devices.

The UEFI proof also probes both virtio PCI and virtio-mmio block transports when firmware never reaches the loader, and captures each attempt separately.

Use `tools/qemu/prove-pr1-software-gate.sh` to run the three proofs sequentially with full compiler/QEMU output redirected to `build/pr1/host-logs`, then check the software gate without re-running product preparation.
