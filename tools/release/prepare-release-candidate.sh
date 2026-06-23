#!/usr/bin/env bash
# Build release-candidate metadata for the current AegisOS v2.0 tree.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

# The formal QEMU proof should be run first on the release host. This helper
# intentionally only packages/verifies the existing proven artifacts.
tools/release/package-release-bundle.py
