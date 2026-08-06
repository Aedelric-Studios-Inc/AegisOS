# PR1 virtio runtime correction

The PR1 Rust runtime reached native PID 1, service-manager, aegisd, dashboard,
and RustMyAdmin, but storage and networking remained unavailable. The guest
log showed a valid FDT followed by zero live virtio transports. Earlier
lifecycle logs had also shown three transports but `AEGIS_EINVAL` from both
block and Ethernet initialisation.

This correction:

- falls back to the canonical QEMU `virt` MMIO window when FDT candidates do
  not resolve to a live transport;
- forces QEMU's virtio-mmio transport into modern (version 2) mode;
- negotiates the mandatory `VIRTIO_F_VERSION_1` feature in block, network and
  RNG drivers;
- negotiates virtio-blk flush support used by persistent AegisFS; and
- preserves partial runtime evidence before the proof validator runs.

This does not manufacture gate markers. Persistence, DHCP and socket evidence
are still accepted only when emitted by the AegisOS guest.
