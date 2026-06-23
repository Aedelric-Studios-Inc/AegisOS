# AegisOS v47 — AegisBox Developer Preview IMG

AegisOS v47 packages the v39-v46 proven kernel/router chain into the AegisBox Developer Preview IMG contract.

## Proved scope

- v46 router control-plane state is available before the Developer Preview contract is prepared.
- Developer Preview manifest state is built in-kernel.
- Lite, Pro, Bastion, and Router variants are registered as enabled image profiles.
- Board profile, service config, installer/flash flow, security baseline, and documentation readiness are tracked.
- The existing EL0 launch/exit proof chain is preserved after the Developer Preview contract.
- Host-side Developer Preview packaging wraps the v40 flash image and emits manifest, checksum, and dev-preview config artifacts.

## Expected QEMU proof line

```text
QEMU v47 proof accepted: AegisBox Developer Preview IMG contract prepared with Lite/Pro/Bastion/Router variants and EL0 launch/exit chain preserved.
```

## Host-side image artifact

After `tools/qemu/build-and-smoke-aarch64.sh` succeeds, v47 emits:

```text
build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img
build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img.manifest.json
build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img.sha256
build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img.dev-preview.json
```

## Boundary

v47 is a Developer Preview image, not the final v55 Release IMG. The remaining v48-v54 line should focus on polish, board profiles, installer/flash flow hardening, service configs, security hardening, docs, and Lite/Pro/Bastion/Router image variants.
