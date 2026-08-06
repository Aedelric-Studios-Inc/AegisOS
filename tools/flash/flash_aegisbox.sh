#!/usr/bin/env bash
# Flash AegisOS image to an AegisBox device.
set -euo pipefail

DEVICE="${1:-/dev/sdX}"
IMAGE="${2:-build/aegisos.img}"

echo "[flash] Writing $IMAGE to $DEVICE"
sudo dd if="$IMAGE" of="$DEVICE" bs=4M conv=fsync status=progress
sudo sync
echo "[flash] Done."
