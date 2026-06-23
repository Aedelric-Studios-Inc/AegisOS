# AegisOS v49 Release Candidate Prep

v49 is the release-candidate preparation layer after the v48 release-polish proof.

## Purpose

v49 does not replace the v48 kernel proof. It adds a release-candidate gate around the existing artifacts:

- Required artifact presence checks
- SHA256 verification
- Release-candidate manifest generation
- QEMU Developer Preview VM helper
- Release checklist and release notes template
- Clear separation between Developer Preview, Release Candidate, and final v55 Release IMG

## v49 success criteria

```text
release candidate manifest generated
required image artifacts present
required SHA256 files verify
v48 release-polish manifest present
Developer Preview VM boot command available
release checklist present
known limitations documented
```

## Promotion boundary

v49 can be called release-candidate-prep after artifact verification passes.

It cannot be called final release until the later release hardening stages are complete and v55 Release IMG is generated.

