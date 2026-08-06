# AegisOS v50–v55 Release Notes

This release train promotes the v48 polished Developer Preview tree into a v55 Release IMG candidate.

## v50 — installer/flash flow hardening

- Adds explicit flash-device confirmation expectations.
- Preserves dry-run validation before destructive flashing.
- Keeps QEMU boot validation as a required pre-release gate.
- Documents that the current AArch64 QEMU path still uses `-kernel` plus an attached raw image.

## v51 — Lite/Pro/Bastion/Router variant images

- Produces release variant image wrappers for Lite, Pro, Bastion, and Router profiles.
- Generates SHA-256 sidecars and per-variant metadata.
- Keeps the Router variant as the source promoted into the v55 Release IMG.

## v52 — security hardening and service defaults freeze

- Freezes default supervised services.
- Keeps deny-inbound firewall posture as the baseline.
- Preserves v45 user/kernel isolation, guard-page, and v44 supervisor/fault proof assumptions.

## v53 — docs, known limitations, and hardware notes

- Adds final release documentation gates.
- Publishes known limitations rather than hiding unfinished bootloader/network-driver work.
- Adds hardware notes for AegisBox family variants.

## v54 — final RC audit/signing/checksums

- Generates release manifests, checksum sidecars, and a signing-manifest placeholder.
- Cryptographic production signing is explicitly reserved for a later signing infrastructure.

## v55 — Release IMG

- Produces `build/images/release-v55/AegisOS-v2.0-v55-release-aarch64.img`.
- Preserves the v46 network control plane, v45 isolation, v44 supervisor, and EL0 launch/exit proof chain.
