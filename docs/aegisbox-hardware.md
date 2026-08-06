# AegisBox Hardware

## Supported platforms

- **AegisBox Lite** — compact edge appliance profile
- **AegisBox Pro** — higher-throughput branch appliance profile
- **AegisBox Bastion** — hardened perimeter appliance profile

## Source of truth

Board-specific implementation remains under `hal/boards/` today, while `hal/board/`, `hal/dtb/`, and `targets/` provide the product-facing structure requested for the repository.
