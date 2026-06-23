#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

export RUST_BACKTRACE=1

if [[ -d vendor ]]; then
  export CARGO_CHECK_FLAGS="--offline"
fi

has_external_deps() {
  local manifest="$1"
  awk '
    /^\[dependencies\]/ { indeps=1; next }
    /^\[/ && indeps { indeps=0 }
    indeps && /^[[:space:]]*[A-Za-z0-9_-]+[[:space:]]*=/ { found=1 }
    END { exit found ? 0 : 1 }
  ' "$manifest"
}

check_rust() {
  echo "== Rust crate checks =="
  while IFS= read -r manifest; do
    if [[ ! -d vendor && "${AEGISOS_CHECK_EXTERNAL_DEPS:-0}" != "1" ]] && has_external_deps "$manifest"; then
      echo "-- skipping external-dependency crate offline: ${manifest%/Cargo.toml}"
      echo "   set AEGISOS_CHECK_EXTERNAL_DEPS=1 or run cargo vendor vendor > .cargo/config.toml to check it"
      continue
    fi
    echo "-- cargo check: ${manifest%/Cargo.toml}"
    cargo check --manifest-path "$manifest" ${CARGO_CHECK_FLAGS:-}
  done < <(find . -name Cargo.toml -not -path '*/target/*' -not -path '*/vendor/*' | sort)
}

check_c() {
  echo "== C syntax checks =="
  while IFS= read -r file; do
    echo "-- clang syntax: $file"
    clang --target=aarch64-none-elf \
      -fsyntax-only \
      -ffreestanding \
      -I kernel/include \
      -I hal/include \
      -I fs/include \
      -I net/include \
      "$file"
  done < <(find kernel hal fs net -name '*.c' | sort)
}


check_kernel_build() {
  if [[ "${AEGISOS_CHECK_KERNEL_BUILD:-0}" != "1" ]]; then
    return
  fi
  echo "== AArch64 kernel link check =="
  tools/build/kernel-clang-aarch64.sh
  file build/aegisos.elf build/aegisos.bin
}

check_asm() {
  echo "== ARM64 assembly syntax checks =="
  while IFS= read -r file; do
    echo "-- clang asm: $file"
    clang --target=aarch64-none-elf -c "$file" -o /tmp/aegisos-asm-check.o
  done < <(find boot kernel hal -name '*.S' | sort)
}

check_rust
check_c
check_asm
check_kernel_build

echo "AegisOS checks completed."
