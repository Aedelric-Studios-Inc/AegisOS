#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
need_grep() {
  local pat="$1" file="$2"
  if ! grep -q "$pat" "$file"; then
    echo "error: v39/AegisOS-v2.0 guard failed: missing '$pat' in $file" >&2
    exit 2
  fi
}
need_file() {
  local file="$1"
  if [[ ! -f "$file" ]]; then
    echo "error: v39/AegisOS-v2.0 guard failed: missing file $file" >&2
    exit 2
  fi
}
if [[ ! -f VERSION ]] || ! grep -Eq 'AegisOS-v2\.0-v39-router-img' VERSION; then
  echo "error: v39/AegisOS-v2.0 guard failed: stale or wrong VERSION" >&2
  exit 2
fi
need_file README.md
need_file docs/AEGISOS_OVERVIEW.md
need_file docs/MILESTONE_HISTORY.md
need_file docs/IMAGE_PACKAGING.md
need_file docs/V2_RELEASE_NOTES.md
need_file tools/image/build-aegisos-v2-img.py
need_file tools/image/build-aegisos-v2-img.sh
need_grep 'AegisOS-v2.0-v39-router-img' README.md
need_grep 'AegisOS-v2.0' docs/AEGISOS_OVERVIEW.md
need_grep 'v39' docs/MILESTONE_HISTORY.md
need_grep 'AegisOS-v2.0-router-aarch64.img' docs/IMAGE_PACKAGING.md
need_grep 'AegisOS v2.0/v39 image packaging accepted' tools/image/build-aegisos-v2-img.py
need_grep 'assert-v39-milestone.sh' tools/qemu/build-and-smoke-aarch64.sh
need_grep 'build-aegisos-v2-img.sh' tools/qemu/build-and-smoke-aarch64.sh
if compgen -G 'README-v*.md' >/dev/null; then
  echo "error: v39/AegisOS-v2.0 guard failed: root milestone README-v*.md files should be consolidated into docs/MILESTONE_HISTORY.md" >&2
  exit 2
fi
echo "v39/AegisOS-v2.0 milestone guard: OK (AegisOS-v2.0-v39-router-img)"
