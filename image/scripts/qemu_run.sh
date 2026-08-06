#!/usr/bin/env bash
set -euo pipefail
exec "$(dirname "$0")/../../tools/qemu/run-aarch64.sh" "$@"
