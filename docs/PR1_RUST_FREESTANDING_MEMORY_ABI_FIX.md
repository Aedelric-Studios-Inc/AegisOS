# PR1 Rust freestanding memory ABI fix

The native Rust build reached all five AegisOS packages and then failed during
final linking because generated AArch64 code referenced ordinary C ABI memory
symbols (`memset` and `memcmp`). AegisOS native userland intentionally has no
hosted libc, so those symbols must be supplied by the AegisOS runtime build.

This change adds `userland/native/memory_abi.S`, exporting:

- `memset`
- `memcpy`
- `memmove`
- `memcmp`
- `bcmp`
- `strlen`

`tools/build/native-rust-aarch64.sh` now compiles that source as a freestanding
AArch64 object and passes its absolute path to each final native Rust ELF link.
The byte loops do not depend on SIMD/FP state, accept unaligned byte addresses,
and cannot be optimised into recursive calls to the symbols they implement.

The source-side link boundary is fixed by this object. The actual Cargo link and
QEMU guest execution remain runtime proofs performed by
`tools/qemu/prove-pr1-runtime-aarch64.sh`.
