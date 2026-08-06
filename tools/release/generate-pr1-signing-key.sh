#!/usr/bin/env bash
set -euo pipefail
out="${1:-build/pr1/keys}"; mkdir -p "$out"; umask 077
openssl genpkey -algorithm ED25519 -out "$out/aegisos-pr1-ed25519-private.pem"
openssl pkey -in "$out/aegisos-pr1-ed25519-private.pem" -pubout -out "$out/aegisos-pr1-ed25519-public.pem"
echo "Development PR1 signing key created under $out. Production keys must be generated and held offline/HSM-backed."
