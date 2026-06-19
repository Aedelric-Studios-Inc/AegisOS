#!/usr/bin/env bash
# AegisOS image builder script
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUTPUT="$SCRIPT_DIR/../build/aegisos.img"

echo "[build_image] Building AegisOS disk image..."

# 1. Create raw image
dd if=/dev/zero of="$OUTPUT" bs=1M count=4608 status=progress

# 2. Partition
parted -s "$OUTPUT" \
    mklabel gpt \
    mkpart boot fat32  1MiB  257MiB \
    mkpart rootfs-a ext4 257MiB 2305MiB \
    mkpart rootfs-b ext4 2305MiB 4353MiB \
    mkpart data ext4 4353MiB 100%

echo "[build_image] Image created: $OUTPUT"
