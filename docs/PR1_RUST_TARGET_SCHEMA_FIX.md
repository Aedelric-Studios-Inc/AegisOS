# PR1 Rust custom-target schema correction

The pinned `nightly-2026-08-04` compiler deserialises both
`target-pointer-width` and `target-c-int-width` as numeric `u16` fields. The
previous checked-in AegisOS target encoded them as JSON strings, so rustc
rejected the target before compiling userland.

The checked-in target now uses numeric values:

```json
"target-pointer-width": 64,
"target-c-int-width": 32
```

Custom-target JSON is an unstable compiler interface. The native Rust build now
exports rustc's own `aarch64-unknown-none-softfloat` target specification,
applies the AegisOS OS/vendor/linker identity, and validates that generated
specification before invoking Cargo. This keeps the ABI, data layout, linker
flavour and JSON field types aligned with the pinned compiler rather than
failing one stale field at a time.
