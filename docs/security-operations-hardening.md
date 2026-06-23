# AegisOS Security Operations Hardening

This pass adds Chapter-12-style Security Operations controls to the AegisBox/Aegis Router production path without pulling PhoenixOS phone complexity into the core.

## Hardened baseline

AegisBox now has a hardened baseline configuration at:

- `system/config/hardening.toml`
- `image/rootfs/etc/hardening.toml`
- `security/policy/hardened_baseline.toml`

The baseline is intentionally practical:

- secure boot required
- signed updates required
- rollback/recovery support required
- audit logging enabled
- strict capability mode
- default-deny inbound firewall posture
- service allow-listing
- least-privilege service policy
- admin tokens required for `aegisd` and RustMyAdmin
- intrusion-watch and traffic-monitor visibility services
- integrity manifest hooks
- secrets kept outside web roots

## aegisd security routes

`aegisd` exposes a local Unix-socket API for security status:

```text
GET  /security/baseline
POST /security/baseline/enforce
GET  /security/audit
GET  /security/incidents
```

`/security/baseline` returns a hardening score and failed controls. `/security/baseline/enforce` is safe-by-default: it creates basic log paths and reports image/service-manager enforcement items rather than silently rewriting network state.

## aegisctl security commands

`aegisctl` now includes:

```text
aegisctl security baseline
aegisctl security audit
aegisctl security incidents
aegisctl security enforce
```

These commands make AegisBox hardening visible from terminal, RustMyAdmin, and later provisioning scripts.

## Kernel/core security vocabulary

The `aegis-security` no_std crate now includes `hardening.rs`, which defines:

- `HardeningControl`
- `HardeningState`
- `evaluate_baseline()`
- `Severity`
- `classify_event()`

This keeps the kernel/core vocabulary reusable for future PhoenixOS work while keeping AegisBox-specific policy in edition/config layers.

## AegisBox first, PhoenixOS later

These controls are meant to make the AegisBox production path safer now:

```text
AegisOS Core
└── AegisBox Edition
    ├── secure boot
    ├── signed updates
    ├── RustMyAdmin admin plane
    ├── LTE/Wi-Fi router services
    ├── firewall/DNS/VPN
    ├── audit logging
    └── incident visibility
```

PhoenixOS can later inherit the core concepts without inheriting appliance-only assumptions.
