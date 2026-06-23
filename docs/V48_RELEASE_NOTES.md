# AegisOS v48 Release Notes

Milestone: **AegisOS-v2.0-v48-release-polish-board-profiles-service-configs**

V48 is the first post-Developer-Preview polish milestone. It keeps the v47
Developer Preview IMG contract and adds kernel-visible release polish gates for
board profiles, service defaults, installer/flash validation, documentation,
and security-hardening readiness.

## Scope

- Board profile matrix for Lite, Pro, Bastion, and Router.
- Per-profile RAM, storage, network-port, and router-capability metadata.
- Service preset matrix for `aegis-init`, `aegisd`, `routerd`, `netd`,
  `firewalld`, `natd`, `storaged`, `rustmyadmin`, and `watchdogd`.
- Restart/log/persistent-config policy markers for the early service set.
- Release polish gates for checksums, flash dry-run, serial console,
  persistent config, service defaults, hardening baseline, and documentation.
- QEMU proof checkpoints proving v48 state while preserving the v46/v47 chain.

## Expected proof line

```text
QEMU v48 proof accepted: release polish gates, board profile matrix, service config presets, security hardening baseline, and docs/manifest checks prepared while preserving the v47 Developer Preview chain.
```

## Boundary

V48 is still not the final v55 Release IMG. It is a polish and configuration
proof milestone that prepares the Developer Preview line for the v48-v54 hardening
run.
