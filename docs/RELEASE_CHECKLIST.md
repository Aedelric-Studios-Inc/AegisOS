# AegisOS v2.0 Release Readiness Checklist

This checklist is for promoting the current v48 release-polish tree toward the v55 Release IMG.

## 1. Required proof chain

Run the full proof chain on the release host:

```bash
cd ~/AegisOS
tools/dev/fix-clock-skew.sh
tools/qemu/build-and-smoke-aarch64.sh
```

The release candidate is not acceptable unless the output contains:

```text
QEMU v46 proof accepted
QEMU v47 proof accepted
QEMU v48 proof accepted
QEMU v40 disk/image boot path accepted
```

## 2. Manual VM boot smoke

Boot the Developer Preview IMG manually with the VM helper:

```bash
tools/qemu/boot-dev-preview-vm-aarch64.sh
```

Expected boot stages:

```text
v45 user/kernel page-table isolation
v46 network bring-up + DHCP + NAT/firewall control plane
v47 AegisBox Developer Preview IMG contract
v48 release-polish gates
EL0 launch/exit proof preserved
```

Quit QEMU with `Ctrl+A`, then `X`.

## 3. Artifact verification

Generate and verify release-candidate metadata:

```bash
tools/release/prepare-release-candidate.sh
tools/release/verify-release-artifacts.sh
```

Required outputs:

```text
build/images/AegisOS-v2.0-router-aarch64-v40-flash.img
build/images/AegisOS-v2.0-router-aarch64-v40-flash.img.sha256
build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img
build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img.sha256
build/images/release-polish/AegisOS-v2.0-v48-release-polish-manifest.json
build/images/release-candidate/AegisOS-v2.0-release-candidate-manifest.json
build/images/release-candidate/AegisOS-v2.0-release-candidate-manifest.sha256
```

## 4. Release blockers before v55

The current v48/v49 release-candidate prep is still pre-release. Do not label it final v55 until these are complete:

- Formal v46/v47/v48 QEMU proof accepted on the release host.
- Release-candidate manifest verifies all required checksums.
- Board profile names and service defaults are frozen.
- Flash instructions are reviewed for the exact target hardware.
- Hardware boot flow is tested on at least one AegisBox-class device or clearly marked QEMU-only Developer Preview.
- Known limitations are written into the release notes.
- Final release package is signed or checksum-pinned.

## 5. Promotion rule

Use this naming boundary:

```text
v48 = release polish proof
v49-v54 = release-candidate hardening, docs, installer, variants, signing, final audit
v55 = Release IMG
```

