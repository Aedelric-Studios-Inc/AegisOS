#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"; cd "$ROOT"
OUT="${AEGISOS_PR1_OUT:-build/pr1/products}"; mkdir -p "$OUT"
export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct 2>/dev/null || echo 0)}" TZ=UTC LC_ALL=C
for spec in router:router pro:aegisbox-pro bastion:bastion; do
  product="${spec%%:*}"; board="${spec##*:}"; build="build/pr1/$product"
  rm -rf "$build"
  BOARD="$board" BUILD_DIR="$build" AEGISOS_FORCE_REBUILD=1 tools/build/kernel-clang-aarch64.sh
  cp "$build/aegisos.elf" "$OUT/AegisOS-2.0.0-pre.1-$product-aarch64.elf"
  cp "$build/aegisos.bin" "$OUT/AegisOS-2.0.0-pre.1-$product-aarch64.bin"
  sha256sum "$OUT/AegisOS-2.0.0-pre.1-$product-aarch64."{elf,bin} > "$OUT/AegisOS-2.0.0-pre.1-$product.sha256"
done
python3 - "$OUT" <<'PY'
import json,sys,hashlib,pathlib,os
out=pathlib.Path(sys.argv[1]); arts=[]
for p in sorted(out.glob('AegisOS-*.*')):
 if p.suffix in {'.elf','.bin'}:
  arts.append({'file':p.name,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size})
root=pathlib.Path.cwd()
profiles=[]
for product in ('router','pro','bastion'):
 p=root/'config'/'products'/f'{product}.toml'
 profiles.append({'product':product,'profile':p.as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})

# A product matrix is invalid if the supposedly distinct product kernels are
# byte-identical.  BOARD_NAME is embedded in each kernel and printed during
# boot, so this assertion catches a regression back to one BOARD_BASTION image
# copied under three labels.
bins=sorted(out.glob('AegisOS-*-aarch64.bin'))
hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in bins}
if len(set(hashes.values())) != len(hashes):
 raise SystemExit('error: PR1 product kernels are not byte-distinct: '+json.dumps(hashes,sort_keys=True))

manifest={'schema':'aegisos-pr1-product-matrix-v1','version':'2.0.0-pre.1','source_date_epoch':os.environ.get('SOURCE_DATE_EPOCH','0'),'profiles':profiles,'artifacts':arts}
(out/'AegisOS-2.0.0-pre.1-product-matrix.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
PY
