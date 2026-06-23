# AegisOS QEMU Entry Diagnosis v17

v16 proved that even the standalone QEMU sanity payloads timed out with empty
output.  That means the problem is still below AegisOS proper: QEMU entry,
firmware/loading, or CPU PC state.

## Main changes

- Generic-loader modes no longer pass `-bios none` by default.
  Some QEMU versions treat `none` as a ROM filename in some contexts, and using
  it made behaviour inconsistent across probes.
- `tools/qemu/probe-entry-aarch64.sh` now starts QEMU paused with `-S`, opens an
  HMP monitor socket, and asks QEMU for `info registers` before and after
  continuing.
- The probe also disassembles around `0x40080000` with `xp /8i 0x40080000` so we
  can see whether the sanity payload is actually loaded where expected.
- Trace scripts now use loader modes without `-bios none` by default.

## Commands

```bash
cd ~/AegisOS

tools/dev/fix-clock-skew.sh
tools/qemu/sanity-aarch64.sh

tools/qemu/probe-entry-aarch64.sh
```

If the smoke test remains silent, the important files are now:

```text
build/qemu-sanity/probe-loader-raw-no-bios.monitor.log
build/qemu-sanity/probe-loader-raw-no-bios.run.log
build/qemu-sanity/probe-loader-raw-no-bios.trace.log
build/qemu-sanity/probe-loader-elf-no-bios.monitor.log
build/qemu-sanity/probe-kernel-image-bin.monitor.log
build/qemu-sanity/probe-bios-bare.monitor.log
```

## Reading the probe

- If `PC` is `0x40080000` in loader-raw mode, QEMU did set CPU0 to the payload.
- If `PC` is not `0x40080000`, the generic loader did not set entry correctly.
- If `xp /8i 0x40080000` cannot disassemble real instructions, the payload is not
  being loaded at the expected address.
- If PC and bytes are correct but no semihost/UART output appears, then the next
  issue is the payload's current exception level, semihost trap handling, or
  UART MMIO path.
