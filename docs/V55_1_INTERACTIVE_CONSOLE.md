# AegisOS v55.1 Interactive Runtime Console

v55.1 adds the first QEMU/Raspberry-Pi-style serial console path for the v55 Release IMG.

This is intentionally a kernel-hosted console bridge. Normal non-smoke boots now prepare the service supervisor, page isolation, v46 network control-plane, v47 Developer Preview, v48 release polish, and v55 release-final state before starting the prompt on `ttyAMA0`. The blocking EL0 proof ladder remains reserved for `AEGISOS_QEMU_SMOKE` builds.

```bash
tools/qemu/boot-release-vm-aarch64.sh
```

Expected prompt:

```text
AegisOS v2.0 Release — v55.1 interactive runtime
aegis:/#
```

Useful commands:

```text
help
status
services
rust
rustmyadmin
net
variants
rootfs
ls /etc
cat /etc/services.toml
cat /opt/rustmyadmin/INTEGRATION.txt
run rustmyadmin
```

## What is connected now

- The runtime initramfs/rootfs bridge installs `/bin/aegis-shell`, `/etc/*.toml`, `/opt/rustmyadmin/INTEGRATION.txt`, and a Rust bridge manifest.
- The shell exposes the kernel service registry, prepared service-supervisor state, prepared user/kernel page-isolation state, v46 network state, v55 release variants, and rootfs file catalogue.
- Rust source trees under `userland/`, `services/`, `rustmyadmin/`, `dashboard/`, and `security/` are catalogued and mapped into the interactive shell as runtime assets.

## Still future work

Native Rust execution is not claimed complete in v55.1. The shell connects/catalogues the Rust code and exposes service/run plans, but actually executing Rust binaries as normal AegisOS processes still requires the AegisOS Rust target, ABI, loader, scheduler promotion, sockets, and process lifetime management.
