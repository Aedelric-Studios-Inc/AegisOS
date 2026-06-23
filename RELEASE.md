# AegisOS v2.0 v55 Release IMG

This tree is promoted to the v55 Release IMG milestone.

Main commands:

```bash
tools/dev/fix-clock-skew.sh
tools/qemu/build-and-smoke-aarch64.sh
tools/release/verify-v55-release.sh
```

Primary release output:

```text
build/images/release-v55/AegisOS-v2.0-v55-release-aarch64.img
build/images/release-v55/AegisOS-v2.0-v55-release-manifest.json
build/images/release-v55/AegisOS-v2.0-v55-release-aarch64.img.sha256
```

The release keeps the known boot boundary: QEMU uses `-kernel` and attaches the release IMG as a raw flash-style disk image. Standalone firmware boot remains future work.
