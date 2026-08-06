#!/usr/bin/env bash
# Verify AegisOS release-candidate files and checksum sidecars.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

required=(
  "build/aegisos.bin"
  "build/aegisos.elf"
  "build/images/AegisOS-v2.0-router-aarch64-v40-flash.img"
  "build/images/AegisOS-v2.0-router-aarch64-v40-flash.img.sha256"
  "build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img"
  "build/images/developer-preview/AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img.sha256"
  "build/images/release-polish/AegisOS-v2.0-v48-release-polish-manifest.json"
  "build/images/release-polish/AegisOS-v2.0-v48-release-polish-manifest.sha256"
  "build/images/release-candidate/AegisOS-v2.0-release-candidate-manifest.json"
  "build/images/release-candidate/AegisOS-v2.0-release-candidate-manifest.sha256"
  "docs/RELEASE_CHECKLIST.md"
  "docs/RELEASE_NOTES_TEMPLATE.md"
  "RELEASE_CANDIDATE.md"
)

for path in "${required[@]}"; do
  if [[ ! -f "$path" ]]; then
    echo "error: missing release artifact: $path" >&2
    exit 2
  fi
done

(
  cd build/images
  sha256sum -c AegisOS-v2.0-router-aarch64-v40-flash.img.sha256
)
(
  cd build/images/developer-preview
  sha256sum -c AegisOS-v2.0-v47-aegisbox-developer-preview-aarch64.img.sha256
)
(
  cd build/images/release-polish
  sha256sum -c AegisOS-v2.0-v48-release-polish-manifest.sha256
)
(
  cd build/images/release-candidate
  sha256sum -c AegisOS-v2.0-release-candidate-manifest.sha256
)

python3 - <<'PY'
import json
from pathlib import Path
manifest = json.loads(Path('build/images/release-candidate/AegisOS-v2.0-release-candidate-manifest.json').read_text())
if manifest.get('status') == 'final':
    raise SystemExit('error: release-candidate manifest incorrectly marked final')
if 'v49-release-candidate-prep' not in manifest.get('milestone', ''):
    raise SystemExit('error: release-candidate milestone missing')
print('AegisOS release-candidate artifact verification accepted.')
PY
