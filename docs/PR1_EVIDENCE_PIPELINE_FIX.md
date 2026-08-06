# PR1 evidence pipeline fix

This patch corrects the PR1 UEFI proof and release-evidence pipeline.

## UEFI proof directory lifetime

`build-pr1-uefi-disk.sh` performs a forced kernel rebuild in the default
`build` directory. That rebuild removes `build/pr1/proofs`. The previous UEFI
proof script created its log files before the rebuild, so the later shell
redirection failed with `No such file or directory`.

The proof now builds the disk first and creates absolute log paths afterwards.
The successful harness line is appended to `uefi-boot.log`.

## Deterministic software evidence

`prepare-pr1-software-evidence.sh` now generates the SPDX SBOM, performs the
two-pass product reproducibility build, and publishes the verified product
matrix. The reproducibility script works outside a Git checkout by using
`SOURCE_DATE_EPOCH=0` when no commit timestamp is available.

## Gate behaviour

The PR1 gate prepares deterministic software evidence by default and reports
software/QEMU blockers separately from physical hardware and independent audit
blockers. `--software-only` checks the automatable gate without weakening the
full PR1 gate.

## Evidence preservation across forced rebuilds

The default Clang kernel builder previously removed the entire `build`
directory. Running the runtime, lifecycle, and UEFI proofs in sequence therefore
deleted evidence from earlier proofs. Forced default builds now invalidate all
normal build outputs while preserving `build/pr1`, which is the dedicated PR1
evidence namespace. The lifecycle proof also writes into
`build/pr1/proofs/native-lifecycle.log` and records its harness pass line there.
