#!/usr/bin/env bash
set -euo pipefail
[[ $# -ge 2 ]] || { echo "usage: $0 PRIVATE_KEY ARTIFACT [SIGNATURE]" >&2; exit 2; }
key="$1"; artifact="$2"; sig="${3:-$artifact.sig}"
[[ -f "$key" && -f "$artifact" ]] || { echo "error: key or artifact missing" >&2; exit 2; }
openssl pkeyutl -sign -inkey "$key" -rawin -in "$artifact" -out "$sig"
sha256sum "$artifact" > "$artifact.sha256"
echo "$sig"
