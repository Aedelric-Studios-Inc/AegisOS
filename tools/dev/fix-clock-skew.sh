#!/usr/bin/env bash
# Normalise file mtimes after ZIP transfer between machines/timezones.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
find . \
  -path './.git' -prune -o \
  -path './target' -prune -o \
  -path './build' -prune -o \
  -exec touch {} +
echo "AegisOS source mtimes normalised. Re-run your build/check command."
