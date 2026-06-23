# QEMU Entry Diagnosis v14

v14 tightens the QEMU bring-up harness after repeated silent timeouts on QEMU
11.0.1.

Changes:

- `loader-raw` now uses one generic-loader device:
  `loader,file=...,addr=...,cpu-num=0,force-raw=on`.
- `loader-elf` now uses one generic-loader device:
  `loader,file=...,cpu-num=0`.
- The standalone sanity payload is now built twice:
  - `sanity_bios.*` linked at `0x0` for `-bios` firmware entry.
  - `sanity_ram.*` linked at `0x40080000` for generic-loader entry.
- Added `tools/qemu/trace-sanity-aarch64.sh` for short instruction traces when
  all banner modes are silent.

Run:

```bash
cd ~/AegisOS
tools/dev/fix-clock-skew.sh
tools/qemu/sanity-aarch64.sh
tools/qemu/build-and-smoke-aarch64.sh
```

If all modes are still silent, run:

```bash
tools/qemu/trace-sanity-aarch64.sh
```

Upload:

- `build/qemu-sanity/trace-loader-raw-bare.log`
- `build/qemu-sanity/trace-loader-raw-bare.run.log`

The trace will show whether CPU0 actually executes at `0x40080000`.
