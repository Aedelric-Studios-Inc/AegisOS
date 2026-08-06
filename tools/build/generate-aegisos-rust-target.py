#!/usr/bin/env python3
"""Generate an AegisOS custom target from rustc's pinned ARM64 soft-float target."""

from __future__ import annotations

import json
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 3:
        print(
            "usage: generate-aegisos-rust-target.py <base-target.json> <output.json>",
            file=sys.stderr,
        )
        return 2

    source = Path(sys.argv[1])
    output = Path(sys.argv[2])
    spec = json.loads(source.read_text(encoding="utf-8"))

    # Keep rustc's own schema, data layout, ABI, linker flavour and numeric field
    # types.  Only apply the AegisOS identity and project linker selection.
    spec["os"] = "aegisos"
    spec["vendor"] = "aedelric"
    spec["env"] = ""
    spec["linker"] = "ld.lld"
    spec["executables"] = True
    spec["panic-strategy"] = "abort"
    spec["relocation-model"] = "static"
    spec["code-model"] = "small"
    spec["position-independent-executables"] = False
    spec["static-position-independent-executables"] = False

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(spec, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
