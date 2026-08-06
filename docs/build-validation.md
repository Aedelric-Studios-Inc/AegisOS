# AegisOS Build Validation

This tree has been checked with an x86_64 GNU Rust toolchain and Clang ARM64 syntax checks.

## Validated locally in production-prep v2

- Rust toolchain: `x86_64-unknown-linux-gnu`
- `cargo check` passed for all checked Rust crates except `tools/sign-image`, which requires external `ed25519-dalek` dependencies that were not vendored in the offline sandbox.
- AArch64 C syntax check passed for `kernel/`, `hal/`, `fs/`, and `net/` C files.
- AArch64 assembly syntax check passed for `boot/`, `kernel/`, and `hal/` assembly files.

## Offline dependency note

`tools/sign-image` depends on `ed25519-dalek`. In offline environments, vendor dependencies before running checks:

```bash
mkdir -p .cargo
cargo vendor vendor > .cargo/config.toml
```

Then run:

```bash
./tools/check-aegisos.sh
```

## Current production target

AegisBox remains the immediate constrained production target. PhoenixOS remains a prepared future layer around the AegisOS kernel/core, not the current build target.
