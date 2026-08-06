#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
CLI="$ROOT/system/cli/aegisctl/target/release/system-aegisctl"

if [ ! -x "$CLI" ]; then
    echo "[router-provision] missing native CLI: $CLI" >&2
    exit 1
fi

attempt=0
while ! "$CLI" health >/dev/null 2>&1; do
    attempt=$((attempt + 1))
    if [ "$attempt" -ge 50 ]; then
        echo "[router-provision] aegisd did not become ready" >&2
        exit 1
    fi
    sleep 0.1
done

result=$($CLI router provision)
printf '%s\n' "$result"

case "$result" in
    *'"ok":false'*)
        echo "[router-provision] provisioning reported a failure" >&2
        exit 1
        ;;
esac

echo "[router-provision] router runtime provisioned"
