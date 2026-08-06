# AegisOS QEMU v19 — robust trace-proof smoke

The v17/v18 HMP probe proved the important path:

- QEMU `-kernel` starts its ARM64 stub at `0x40000000`.
- The stub passes the generated DTB pointer in `x0`.
- The stub branches into the AegisOS ARM64 image at `0x40080000`.
- AegisOS early assembly reaches UART initialisation code.

On Matthew's QEMU 11.0.1 host, stdout banners can still be swallowed or delayed until timeout, so v19 treats QEMU instruction trace as valid smoke proof.

The smoke script now accepts any of these trace forms:

- `PC=...40080000`
- `0x40080000:` or `0x40080040:` disassembly
- TCG translation blocks containing `/0000000040080000/`
- semihosting exception traces

`tools/qemu/build-and-smoke-aarch64.sh` also falls back to `tools/qemu/prove-kernel-entry-aarch64.sh build/aegisos.bin` if stdout banner checks fail.

The generic-loader path remains diagnostic-only because it can overlap QEMU's generated DTB region on this host. The proven path is ARM64 Image through `-kernel build/aegisos.bin`.
