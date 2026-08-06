# AegisOS v16 QEMU Entry Diagnosis

v16 narrows the silent-QEMU problem further.

What changed:

- QEMU now forces TCG explicitly with `-accel tcg,thread=single`.
- The default machine string is now `virt,secure=off,virtualization=off,gic-version=2`.
- Sanity tests now try a semihost-only payload first.
- The semihost-only payload calls `SYS_WRITE0` and then `SYS_EXIT`, so if CPU execution reaches the payload QEMU should print or exit instead of silently looping.
- Trace scripts no longer use the removed `-singlestep` option.
- Added `tools/qemu/probe-entry-aarch64.sh` for a deterministic QEMU/QMP liveness probe.

Run:

```sh
cd ~/AegisOS
tools/dev/fix-clock-skew.sh
tools/qemu/sanity-aarch64.sh
```

If still silent:

```sh
tools/qemu/trace-sanity-aarch64.sh
tools/qemu/trace-variants-aarch64.sh
tools/qemu/probe-entry-aarch64.sh
```

Interpretation:

- Semihost sanity prints/exits: CPU entry is good; UART/AegisOS path is next.
- Semihost sanity times out with empty trace: QEMU is still not entering payload or trace output is not flushing.
- Probe script shows QEMU alive but trace empty: focus on QEMU loader/reset configuration.
