# AegisOS v15 QEMU Trace Fix

## What changed

QEMU 11.0.1 on the test laptop rejects the old `-singlestep` option:

```text
qemu-system-aarch64: -singlestep: invalid option
```

That meant v14's trace tool failed before QEMU could produce an instruction trace.

v15 removes `-singlestep` and uses QEMU TCG trace flags instead:

```text
-d in_asm,exec,cpu,int,guest_errors,unimp
```

## Commands

```bash
tools/qemu/trace-sanity-aarch64.sh
```

If that still produces an empty trace, run the broader set:

```bash
tools/qemu/trace-variants-aarch64.sh
```

Upload the generated `build/qemu-sanity/trace-*.log` and `trace-*.run.log` files.

## Meaning

If the trace log contains instructions at or near `0x40080000`, CPU0 is executing our payload and the UART/semihost call path is the next bug.

If the trace log contains no translated blocks, QEMU never begins executing the payload, so the issue is loader/reset/PC setup.
