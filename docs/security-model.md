# AegisOS Security Model

## Principles

1. **Least Privilege** — every component holds only the capabilities it requires.
2. **Capability-based Access Control** — all kernel resources are accessed via unforgeable capability tokens.
3. **Sandboxing** — userland processes and services run in isolated sandboxes enforced by the kernel and Rust security layer.
4. **Secure Boot** — cryptographic verification of each boot stage prevents unauthorized firmware execution.
5. **Audit Logging** — security-relevant events are recorded to an append-only log.
6. **Integrity Checking** — filesystem and binary integrity is verified at mount/exec time.
7. **Policy Engine** — fine-grained runtime policy controls what services and processes may do.

## Rust Security Crate (`security/`)

| Module              | Responsibility                                   |
|---------------------|--------------------------------------------------|
| `capabilities.rs`   | Capability token creation, delegation, revocation |
| `sandbox.rs`        | Process sandbox enforcement                      |
| `audit.rs`          | Security event audit logging                     |
| `keyring.rs`        | Cryptographic key storage and management         |
| `integrity.rs`      | Binary and filesystem integrity verification     |
| `secure_boot.rs`    | Bootchain signature verification                 |
| `policy_engine.rs`  | Runtime policy evaluation                        |

## Threat Model

AegisOS assumes a hostile network environment. All inbound traffic is filtered by the firewall
service before reaching any application. VPN and DNS-filtering services provide additional
network-layer protection. The intrusion-watch service monitors for anomalous behavior.
