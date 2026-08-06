# PR1 strict-alignment kernel fix

## Fault isolated from QEMU evidence

The lifecycle proof reached writable AegisFS and then trapped before update
metadata initialisation completed:

- `ESR=0x96000061`
- `FSC=0x21 (Alignment fault)`
- `WnR=1`
- `FAR=0x42083d4c`
- `ELR` resolved inside the private `store()` routine in
  `kernel/core/update.c`

`update_disk_t` was packed and therefore had an ABI alignment of one byte.  A
local 512-byte instance was placed at a stack address that was four bytes off
an eight-byte boundary.  Clang emitted a wide store for the `generation`
field, producing an unaligned 64-bit write.

## Repair

1. Compile all AArch64 kernel C sources with `-mstrict-align`.  Packed disk,
   network and device records are now lowered without unsafe unaligned wide
   accesses.
2. Give `update_disk_t` an explicit eight-byte object alignment while keeping
   its packed 512-byte on-disk layout unchanged.
3. Add compile-time assertions for the record size, type alignment and
   multi-byte field offsets.
4. Export the freestanding C ABI memory routines that Clang may emit while
   compiling with strict alignment: `memcpy`, `memset`, `memmove`, `memcmp`,
   `bcmp` and `strlen`.  Every implementation uses byte accesses and is safe
   before normal-memory MMU mappings are established.

## Validation performed

- Complete Bastion AArch64 lifecycle kernel compiled and linked.
- `update_disk_t` remained exactly 512 bytes.
- Post-fix disassembly placed the local record at `sp + 0x38`; its 64-bit
  `generation` field is stored at aligned `sp + 0x48`.
- The final kernel exports all required freestanding memory ABI symbols.
- Kernel and security host unit tests passed.

QEMU guest execution remains the authoritative proof that the runtime moves
past update initialisation and into DHCP/socket/Rust service checks.
