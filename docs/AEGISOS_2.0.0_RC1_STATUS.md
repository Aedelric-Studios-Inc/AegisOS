# AegisOS 2.0.0 RC1

## Verified

- PR1 native-lifecycle proof passed
- PR1 runtime proof passed
- PR1 AAVMF/UEFI proof passed
- Full PR1 software gate passed
- Native Rust PID 1 launched successfully
- Service manager and aegisd supervision passed
- Dashboard and RustMyAdmin listeners started
- DHCP and native socket proof passed
- Virtual NVMe detected through AAVMF/UEFI
- Virtual NVMe installation completed
- AEGIS_BOOT, AEGIS_ROOT, and AEGIS_CONFIG persisted after reboot
- AegisFS remounted writable after reboot
- Clean reboot passed
- Clean PSCI poweroff passed

## Pending External Validation

- Physical AegisBox Pro NVMe installation
- Physical AegisBox Bastion NVMe installation
- Router network soak
- Physical power-loss recovery test
- Independent security review

The pending external validation does not invalidate the completed software and
virtual-hardware proofs. This release candidate must not yet be presented as the
final production release.
