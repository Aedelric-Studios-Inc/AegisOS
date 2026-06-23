# Offline Vendored Build Validation

This package includes vendored Rust dependencies for `tools/sign-image`, including `ed25519-dalek`.

Validated in an offline environment with an x86_64 GNU Rust toolchain:

- `cargo check --manifest-path tools/sign-image/Cargo.toml --offline`: passed
- `./tools/check-aegisos.sh`: passed
- C syntax checks for kernel/HAL/FS/net: passed
- ARM64 assembly syntax checks: passed

Notes:

- The check script intentionally excludes `vendor/` directories from crate discovery.
- Some Rust crates still emit dead-code warnings because prototype modules are present but not yet fully wired into the runtime.
