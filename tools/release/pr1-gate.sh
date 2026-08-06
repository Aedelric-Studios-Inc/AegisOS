#!/usr/bin/env bash
# Fail-closed AegisOS 2.0.0-pre.1 release gate.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

SOFTWARE_ONLY=0
PREPARE_SOFTWARE="${AEGISOS_PR1_PREPARE_SOFTWARE:-1}"
for arg in "$@"; do
  case "$arg" in
    --software-only) SOFTWARE_ONLY=1 ;;
    --no-prepare) PREPARE_SOFTWARE=0 ;;
    -h|--help)
      cat <<'USAGE'
Usage: tools/release/pr1-gate.sh [--software-only] [--no-prepare]

  --software-only  Check build and QEMU evidence without hardware/audit gates.
  --no-prepare     Do not generate deterministic SBOM/product/repro evidence.
USAGE
      exit 0
      ;;
    *)
      echo "error: unknown argument: $arg" >&2
      exit 2
      ;;
  esac
done

software_artifacts=(
  build/pr1/AegisOS-2.0.0-pre.1.spdx.json
  build/pr1/reproducible-build.proof
  build/pr1/products/AegisOS-2.0.0-pre.1-product-matrix.json
)
software_artifacts_missing=0
for file in "${software_artifacts[@]}"; do
  [[ -s "$file" ]] || software_artifacts_missing=1
done
if [[ "$PREPARE_SOFTWARE" == 1 && "$software_artifacts_missing" == 1 ]]; then
  bash tools/release/prepare-pr1-software-evidence.sh
fi

fail=0
software_fail=0
external_fail=0

need_software_file() {
  [[ -s "$1" ]] || {
    echo "BLOCKED [software]: missing $1"
    fail=1
    software_fail=1
  }
}
need_software_line() {
  [[ -s "$1" ]] && grep -Fq "$2" "$1" || {
    echo "BLOCKED [software]: $1 lacks: $2"
    fail=1
    software_fail=1
  }
}
need_external_file() {
  [[ -s "$1" ]] || {
    echo "BLOCKED [external]: missing $1"
    fail=1
    external_fail=1
  }
}

need_software_file build/pr1/AegisOS-2.0.0-pre.1.spdx.json
need_software_file build/pr1/reproducible-build.proof
need_software_file build/pr1/products/AegisOS-2.0.0-pre.1-product-matrix.json
need_software_line build/pr1/proofs/native-lifecycle.log \
  "AegisOS v58 native lifecycle/recovery proof passed."
need_software_line build/pr1/proofs/uefi-boot.log \
  "AegisOS PR1 UEFI disk boot proof passed."
need_software_line build/pr1/proofs/persistence.log \
  "AegisFS persistence proof passed"
need_software_line build/pr1/proofs/network.log \
  "DHCP bound"
need_software_line build/pr1/proofs/socket.log \
  "native socket proof passed"
need_software_line build/pr1/proofs/rust-userspace.log \
  "native Rust PID 1 online"
need_software_line build/pr1/proofs/rustmyadmin.log \
  "[rustmyadmin:rust] listening on 0.0.0.0:8081"

if ((SOFTWARE_ONLY == 0)); then
  need_external_file build/pr1/security/security-review.md
  need_external_file build/pr1/hardware/pro-nvme-install.log
  need_external_file build/pr1/hardware/bastion-nvme-install.log
  need_external_file build/pr1/hardware/router-network-soak.log
  need_external_file build/pr1/hardware/power-loss-recovery.log
fi

if ((fail)); then
  if ((software_fail)); then
    echo "AegisOS 2.0.0-pre.1 software gate: BLOCKED"
  else
    echo "AegisOS 2.0.0-pre.1 software gate: PASSED"
  fi
  if ((SOFTWARE_ONLY == 0)); then
    if ((external_fail)); then
      echo "AegisOS 2.0.0-pre.1 external validation gate: BLOCKED"
    else
      echo "AegisOS 2.0.0-pre.1 external validation gate: PASSED"
    fi
  fi
  echo "AegisOS 2.0.0-pre.1 gate: BLOCKED"
  exit 1
fi

if ((SOFTWARE_ONLY)); then
  echo "AegisOS 2.0.0-pre.1 software gate: PASSED"
else
  echo "AegisOS 2.0.0-pre.1 software gate: PASSED"
  echo "AegisOS 2.0.0-pre.1 external validation gate: PASSED"
  echo "AegisOS 2.0.0-pre.1 gate: PASSED"
fi
