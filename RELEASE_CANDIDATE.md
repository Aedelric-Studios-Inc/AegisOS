# AegisOS v2.0 Release Candidate Notes

This tree is prepared for release-candidate validation, not final public v55 release.

## Fast path

```bash
tools/dev/fix-clock-skew.sh
tools/qemu/build-and-smoke-aarch64.sh
tools/release/prepare-release-candidate.sh
tools/release/verify-release-artifacts.sh
```

## Manual VM boot

```bash
tools/qemu/boot-dev-preview-vm-aarch64.sh
```

## Final release rule

Do not publish as final v55 until:

- QEMU proof is accepted on the release host.
- Release-candidate checksums verify.
- Board and flash instructions are reviewed.
- Known limitations are included in release notes.
- Final release package is checksum-pinned or signed.

