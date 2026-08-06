#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 3 ]] || { echo "usage: $0 PUBLIC_KEY ARTIFACT SIGNATURE" >&2; exit 2; }
openssl pkeyutl -verify -pubin -inkey "$1" -rawin -in "$2" -sigfile "$3"
