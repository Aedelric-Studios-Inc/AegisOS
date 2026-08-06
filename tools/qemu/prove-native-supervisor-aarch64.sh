#!/usr/bin/env bash
# v58 supersedes the v57 supervisor-only proof with lifecycle/recovery proof.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
exec "$ROOT/tools/qemu/prove-native-lifecycle-aarch64.sh" "$@"
