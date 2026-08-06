# PR1 UEFI PE relocation fix

The ARM64 UEFI application previously contained no PE/COFF base-relocation
directory. `lld-link` therefore marked the application with a preferred image
base of `0x140000000` but emitted no `.reloc` section. That address is outside
the 1 GiB QEMU `virt` RAM window (`0x40000000`-`0x7fffffff`). EDK2 rejected the
application before `efi_main` was entered, which is why both proof logs stayed
empty and no `[AegisOS:uefi]` line appeared.

The loader now contains one retained absolute relocation anchor, links with an
in-range preferred base, and is checked by `validate-uefi-pe.py`. The build
fails unless the output is an ARM64 PE32+ EFI application with a non-empty
base-relocation directory backed by `.reloc`.
