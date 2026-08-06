#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

command -v cargo >/dev/null || {
  echo "error: Cargo/rustup is required" >&2
  exit 2
}
command -v ld.lld >/dev/null || {
  echo "error: ld.lld is required" >&2
  exit 2
}
command -v clang >/dev/null || {
  echo "error: clang is required for the AArch64 freestanding ABI object" >&2
  exit 2
}

TARGET_SOURCE="$ROOT/targets/aarch64-unknown-aegisos.json"
WORKSPACE_MANIFEST="$ROOT/userland/Cargo.toml"
LINKER_SCRIPT="$ROOT/userland/native/aegis-user.ld"
MEMORY_ABI_SOURCE="$ROOT/userland/native/memory_abi.S"
BUILD_DIR="${BUILD_DIR:-build}"
TARGET_DIR="$ROOT/$BUILD_DIR/targets"
TARGET_BASE="$TARGET_DIR/aarch64-unknown-none-softfloat.json"
TARGET="$TARGET_DIR/aarch64-unknown-aegisos.json"
OUT="$ROOT/$BUILD_DIR/userland/native-rust"
MEMORY_ABI_OBJECT="$OUT/aegis_memory_abi.o"
GENERATED="$ROOT/$BUILD_DIR/generated"
CARGO_TARGET_DIR="${CARGO_TARGET_DIR:-$ROOT/$BUILD_DIR/cargo-target}"
export CARGO_TARGET_DIR

# The linker script and memory ABI object are deliberately supplied as absolute
# paths. Cargo may invoke rustc/linker from a package directory.
mkdir -p "$OUT" "$GENERATED" "$TARGET_DIR"

echo "Building AegisOS freestanding AArch64 memory ABI..."
clang --target=aarch64-none-elf \
  -ffreestanding -nostdlib -fno-stack-protector -fno-PIE \
  -c "$MEMORY_ABI_SOURCE" -o "$MEMORY_ABI_OBJECT"

AEGIS_LINK_FLAGS="-C link-arg=-T$LINKER_SCRIPT -C link-arg=$MEMORY_ABI_OBJECT -C link-arg=-z -C link-arg=max-page-size=4096"
export RUSTFLAGS="${RUSTFLAGS:+$RUSTFLAGS }$AEGIS_LINK_FLAGS"

# Generate the actual Cargo target from the pinned compiler's own built-in
# AArch64 soft-float specification.  Custom-target JSON is unstable, so this
# preserves the exact schema and field types expected by this nightly instead
# of letting a stale hand-written JSON file fail one field at a time.
echo "Generating compiler-matched AegisOS Rust target specification..."
rustc -Z unstable-options \
  --target aarch64-unknown-none-softfloat \
  --print target-spec-json > "$TARGET_BASE"
python3 tools/build/generate-aegisos-rust-target.py "$TARGET_BASE" "$TARGET"

# Keep the checked-in target readable and type-correct as documentation/fallback.
python3 -m json.tool "$TARGET_SOURCE" >/dev/null

echo "Validating generated AegisOS Rust target specification..."
rustc -Z unstable-options --target "$TARGET" --print cfg >/dev/null

packages=(
  aegis-init-native
  service-manager-native
  aegisd-native
  dashboard-native
  rustmyadmin-native
)

cargo_args=(
  build
  # Rust nightly now gates JSON custom targets separately from build-std.
  # Keep this Cargo flag next to build-std so Cargo forwards the required
  # unstable target-spec permission to rustc for targets/*.json.
  -Z json-target-spec
  -Z build-std=core,alloc
  --release
  --locked
  --target "$TARGET"
  --manifest-path "$WORKSPACE_MANIFEST"
)
for package in "${packages[@]}"; do
  cargo_args+=(--package "$package")
done

cargo "${cargo_args[@]}"

embed_one() {
  local bin="$1" symbol="$2" header="$3" expected_marker="$4"
  local src="$CARGO_TARGET_DIR/aarch64-unknown-aegisos/release/$bin"

  if [[ ! -f "$src" ]]; then
    src="$(find "$CARGO_TARGET_DIR" -type f -path '*/release/'"$bin" -print -quit)"
  fi

  [[ -n "$src" && -f "$src" ]] || {
    echo "error: Cargo did not produce $bin" >&2
    exit 2
  }
  if ! grep -aFq "$expected_marker" "$src"; then
    echo "error: $src does not contain expected Rust marker: $expected_marker" >&2
    exit 2
  fi

  cp "$src" "$OUT/$bin.elf"
  python3 tools/build/embed-binary.py "$OUT/$bin.elf" "$GENERATED/$header" "$symbol"
}

embed_one aegis-init aegis_native_init_elf aegis_init_elf.h "[aegis-init:rust] native Rust PID 1 online under AegisOS"
embed_one service-manager aegis_native_service_manager_elf aegis_service_manager_elf.h "[service-manager:rust] native Rust supervisor online"
embed_one aegisd aegis_native_aegisd_elf aegis_aegisd_elf.h "[aegisd:rust] native Rust daemon online"
embed_one dashboard aegis_native_dashboard_elf aegis_dashboard_elf.h "[dashboard:rust] listening on 0.0.0.0:8080"
embed_one rustmyadmin aegis_native_rustmyadmin_elf aegis_rustmyadmin_elf.h "[rustmyadmin:rust] listening on 0.0.0.0:8081"

printf 'AegisOS native Rust EL0 binaries built and embedded in %s.\n' "$BUILD_DIR"
