# AegisOS v2.0 Release Notes

AegisOS-v2.0 is the first image-packaged router bring-up line. It consolidates the v27–v38 boot/userland proofs and adds v39 image packaging plus documentation cleanup.

## Highlights

- File-backed `/sbin/aegis-init` through VFS/initramfs.
- ELF64/AArch64 validation.
- Executable `PT_LOAD` byte copy into user image backing.
- BSS/tail zeroing proof.
- User/kernel permission metadata proof.
- argv/envp-style initial stack bootstrap metadata.
- Multiple planned user process descriptors.
- Deterministic raw `.img` package generation.
- Unified root README and consolidated milestone documentation.

## Output

```text
build/images/AegisOS-v2.0-router-aarch64.img
```
