#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

if [[ -z "${SOURCE_DATE_EPOCH:-}" ]]; then
  SOURCE_DATE_EPOCH="$(git log -1 --format=%ct 2>/dev/null || printf '0')"
fi
export SOURCE_DATE_EPOCH TZ=UTC LC_ALL=C PYTHONHASHSEED=0

rm -rf build/pr1/repro-a build/pr1/repro-b
AEGISOS_PR1_OUT=build/pr1/repro-a/products tools/release/build-pr1-product-matrix.sh
AEGISOS_PR1_OUT=build/pr1/repro-b/products tools/release/build-pr1-product-matrix.sh

mapfile -t artifacts < <(
  find build/pr1/repro-a/products -maxdepth 1 -type f \
    \( -name '*.elf' -o -name '*.bin' -o -name '*.json' \) \
    -printf '%f\n' | sort
)

if ((${#artifacts[@]} == 0)); then
  echo "error: reproducibility build produced no PR1 artifacts" >&2
  exit 2
fi

for file in "${artifacts[@]}"; do
  [[ -f "build/pr1/repro-b/products/$file" ]] || {
    echo "error: second reproducibility pass lacks: $file" >&2
    exit 2
  }
  cmp "build/pr1/repro-a/products/$file" \
      "build/pr1/repro-b/products/$file" || {
    echo "non-reproducible: $file" >&2
    exit 1
  }
done

# Publish the byte-verified first pass as the product matrix consumed by PR1.
rm -rf build/pr1/products
mkdir -p build/pr1/products
cp -a build/pr1/repro-a/products/. build/pr1/products/

mkdir -p build/pr1
printf 'AegisOS PR1 product kernels reproducible at SOURCE_DATE_EPOCH=%s\n' \
  "$SOURCE_DATE_EPOCH" > build/pr1/reproducible-build.proof
