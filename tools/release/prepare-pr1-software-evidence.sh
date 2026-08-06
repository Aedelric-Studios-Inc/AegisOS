#!/usr/bin/env bash
# Materialise deterministic build evidence that does not require QEMU or hardware.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

command -v python3 >/dev/null || {
  echo "error: python3 not found" >&2
  exit 127
}
command -v clang >/dev/null || {
  echo "error: clang not found" >&2
  exit 127
}
command -v ld.lld >/dev/null || {
  echo "error: ld.lld not found" >&2
  exit 127
}
command -v llvm-objcopy >/dev/null || {
  echo "error: llvm-objcopy not found" >&2
  exit 127
}

mkdir -p build/pr1 build/pr1/proofs

tools/release/reproducible-pr1-build.sh
python3 tools/release/generate-sbom.py \
  --root . \
  --output build/pr1/AegisOS-2.0.0-pre.1.spdx.json

required=(
  build/pr1/AegisOS-2.0.0-pre.1.spdx.json
  build/pr1/reproducible-build.proof
  build/pr1/products/AegisOS-2.0.0-pre.1-product-matrix.json
)
for file in "${required[@]}"; do
  [[ -s "$file" ]] || {
    echo "error: PR1 software evidence was not created: $file" >&2
    exit 2
  }
done

printf 'AegisOS PR1 deterministic software evidence prepared.\n'
