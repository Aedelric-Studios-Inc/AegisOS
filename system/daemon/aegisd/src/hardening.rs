//! Hardened Security Operations baseline for AegisBox.
//!
//! These checks model practical controls from a Security Operations chapter:
//! secure configuration baselines, least privilege, default-deny networking,
//! audit logging, integrity monitoring, update discipline, and incident triage.

use std::env;
use std::fs;
use std::os::unix::fs::PermissionsExt;
use std::path::Path;
use std::process::Command;

#[derive(Clone, Copy)]
struct BaselineCheck {
    id: &'static str,
    description: &'static str,
    passed: bool,
    severity: &'static str,
    remediation: &'static str,
}

const AUDIT_LOG: &str = "/var/log/aegis/audit.log";
const INCIDENT_LOG: &str = "/var/log/aegis/incidents.log";
const BASELINE_CONFIG: &str = "/etc/aegis/hardening.toml";
const FALLBACK_BASELINE_CONFIG: &str = "system/config/hardening.toml";

pub fn get_baseline() -> String {
    let checks = collect_checks();
    let total = checks.len();
    let passed = checks.iter().filter(|c| c.passed).count();
    let critical_failures = checks
        .iter()
        .filter(|c| !c.passed && c.severity == "critical")
        .count();
    let high_failures = checks
        .iter()
        .filter(|c| !c.passed && c.severity == "high")
        .count();
    let score = if total == 0 {
        0
    } else {
        (passed * 100) / total
    };
    let status = if critical_failures > 0 {
        "blocked"
    } else if high_failures > 0 {
        "needs_attention"
    } else if passed == total {
        "hardened"
    } else {
        "partial"
    };

    let entries: Vec<String> = checks.iter().map(check_to_json).collect();
    format!(
        r#"{{"profile":"aegisbox-hardened","status":"{}","score":{},"passed":{},"total":{},"critical_failures":{},"high_failures":{},"checks":[{}]}}"#,
        status,
        score,
        passed,
        total,
        critical_failures,
        high_failures,
        entries.join(",")
    )
}

pub fn enforce_baseline() -> String {
    // This daemon route is intentionally safe-by-default. It creates the local
    // audit/log directories and reports the commands/policies that the image
    // builder or service manager should enforce. It does not silently rewrite
    // firewall/network state from the API layer.
    let mut actions = Vec::new();
    if fs::create_dir_all("/var/log/aegis").is_ok() {
        actions
            .push(r#"{"id":"log_dir","applied":true,"detail":"/var/log/aegis exists"}"#.to_owned());
    } else {
        actions.push(
            r#"{"id":"log_dir","applied":false,"detail":"could not create /var/log/aegis"}"#
                .to_owned(),
        );
    }

    if !Path::new(AUDIT_LOG).exists() {
        let applied = fs::OpenOptions::new()
            .create(true)
            .append(true)
            .open(AUDIT_LOG)
            .is_ok();
        actions.push(format!(
            r#"{{"id":"audit_log","applied":{},"detail":"{}"}}"#,
            applied, AUDIT_LOG
        ));
    }

    actions.push(r#"{"id":"firewall_default_deny","applied":false,"detail":"enforced by aegis-firewall/system image policy"}"#.to_owned());
    actions.push(r#"{"id":"signed_updates","applied":false,"detail":"enforced by update-manager/sign-image tooling"}"#.to_owned());
    actions.push(r#"{"id":"service_allow_list","applied":false,"detail":"enforced by system/config/services.toml"}"#.to_owned());

    format!(
        r#"{{"enforce_mode":"safe-report","actions":[{}],"baseline":{}}}"#,
        actions.join(","),
        get_baseline()
    )
}

pub fn get_audit() -> String {
    let lines = read_tail(AUDIT_LOG, 100);
    let entries: Vec<String> = lines
        .into_iter()
        .map(|line| format!("\"{}\"", json_escape(&line)))
        .collect();
    format!(
        r#"{{"source":"{}","events":[{}]}}"#,
        AUDIT_LOG,
        entries.join(",")
    )
}

pub fn get_incidents() -> String {
    let mut lines = read_tail(INCIDENT_LOG, 100);
    lines.extend(
        read_tail("/var/log/aegis/services.log", 100)
            .into_iter()
            .filter(|l| is_incident_line(l)),
    );

    let mut critical = 0usize;
    let mut high = 0usize;
    let mut medium = 0usize;
    let entries: Vec<String> = lines
        .into_iter()
        .map(|line| {
            let sev = classify_line(&line);
            match sev {
                "critical" => critical += 1,
                "high" => high += 1,
                "medium" => medium += 1,
                _ => {}
            }
            format!(
                r#"{{"severity":"{}","message":"{}"}}"#,
                sev,
                json_escape(&line)
            )
        })
        .collect();

    format!(
        r#"{{"critical":{},"high":{},"medium":{},"incidents":[{}]}}"#,
        critical,
        high,
        medium,
        entries.join(",")
    )
}

fn collect_checks() -> Vec<BaselineCheck> {
    vec![
        BaselineCheck {
            id: "secure_boot_enabled",
            description: "Secure boot must be enabled for production images",
            passed: config_contains("secure_boot", "true"),
            severity: "critical",
            remediation: "set [security].secure_boot = true and verify boot-stage signatures",
        },
        BaselineCheck {
            id: "audit_logging_enabled",
            description: "Audit logging must be enabled and writable",
            passed: config_contains("audit_log", "true") && writable_parent(AUDIT_LOG),
            severity: "high",
            remediation:
                "enable audit_log and ensure /var/log/aegis is writable by root/aegis services",
        },
        BaselineCheck {
            id: "strict_capability_mode",
            description: "Capability mode must default to strict/least-privilege",
            passed: config_contains("capability_mode", "strict"),
            severity: "critical",
            remediation: "set capability_mode = strict and deny unlabelled capability grants",
        },
        BaselineCheck {
            id: "aegisd_token_required",
            description: "aegisd must fail closed unless AEGISD_TOKEN is configured",
            passed: env::var("AEGISD_TOKEN")
                .map(|v| !v.trim().is_empty())
                .unwrap_or(false)
                || env::var("AEGISD_DEV_TRUST_LOCAL").is_err(),
            severity: "critical",
            remediation: "set AEGISD_TOKEN in production and do not set AEGISD_DEV_TRUST_LOCAL",
        },
        BaselineCheck {
            id: "rustmyadmin_token_required",
            description: "RustMyAdmin must require an admin token/session in production",
            passed: env::var("RUSTMYADMIN_TOKEN")
                .map(|v| !v.trim().is_empty())
                .unwrap_or(false)
                || env::var("RUSTMYADMIN_DEV_TRUST_LOOPBACK").is_err(),
            severity: "critical",
            remediation: "set RUSTMYADMIN_TOKEN and disable dev loopback trust",
        },
        BaselineCheck {
            id: "default_deny_firewall",
            description: "Inbound firewall posture must be default deny",
            passed: file_contains_any(
                &["/etc/aegis/firewall.toml", "system/config/firewall.toml"],
                &["action = \"drop\"", "action = \"DROP\""],
            ),
            severity: "critical",
            remediation: "keep inbound default DROP and add explicit allow rules only where needed",
        },
        BaselineCheck {
            id: "intrusion_watch_service",
            description: "Intrusion-watch service must be included in the service allow-list",
            passed: file_contains_any(
                &["/etc/aegis/services.toml", "system/config/services.toml"],
                &["aegis-intrusion-watch"],
            ),
            severity: "high",
            remediation: "add aegis-intrusion-watch to services.toml and enable restart policy",
        },
        BaselineCheck {
            id: "traffic_monitor_service",
            description: "Traffic monitor must be available for flow visibility",
            passed: file_contains_any(
                &["/etc/aegis/services.toml", "system/config/services.toml"],
                &["aegis-traffic-monitor"],
            ),
            severity: "medium",
            remediation: "add aegis-traffic-monitor to services.toml",
        },
        BaselineCheck {
            id: "signed_update_tooling",
            description: "Signed image/update tooling must exist",
            passed: Path::new("tools/sign-image/Cargo.toml").exists()
                || command_available("aegis-sign-image"),
            severity: "high",
            remediation: "ship sign-image tooling and reject unsigned update manifests",
        },
        BaselineCheck {
            id: "recovery_partition_defined",
            description: "Recovery partition must be present in the image layout",
            passed: file_contains_any(&["image/partitions.toml"], &["recovery"]),
            severity: "high",
            remediation: "define recovery partition in image/partitions.toml",
        },
        BaselineCheck {
            id: "baseline_config_present",
            description: "Hardened baseline config should be present for drift detection",
            passed: Path::new(BASELINE_CONFIG).exists()
                || Path::new(FALLBACK_BASELINE_CONFIG).exists(),
            severity: "medium",
            remediation: "ship /etc/aegis/hardening.toml from system/config/hardening.toml",
        },
        BaselineCheck {
            id: "kernel_log_not_world_writable",
            description: "Aegis logs must not be world-writable",
            passed: !is_world_writable("/var/log/aegis") && !is_world_writable(AUDIT_LOG),
            severity: "high",
            remediation: "set /var/log/aegis to 0750 and logs to 0640 root:aegis",
        },
    ]
}

fn check_to_json(c: &BaselineCheck) -> String {
    format!(
        r#"{{"id":"{}","description":"{}","passed":{},"severity":"{}","remediation":"{}"}}"#,
        json_escape(c.id),
        json_escape(c.description),
        c.passed,
        json_escape(c.severity),
        json_escape(c.remediation)
    )
}

fn config_contains(key: &str, value: &str) -> bool {
    let needles = [
        format!("{} = \"{}\"", key, value),
        format!("{} = {}", key, value),
    ];
    file_contains_any(
        &[
            "/etc/aegis/aegisos.toml",
            "system/config/aegisos.toml",
            "image/rootfs/etc/aegisos.toml",
        ],
        &[&needles[0], &needles[1]],
    )
}

fn file_contains_any(paths: &[&str], needles: &[&str]) -> bool {
    paths.iter().any(|path| {
        fs::read_to_string(path)
            .map(|s| {
                let lower = s.to_ascii_lowercase();
                needles
                    .iter()
                    .any(|n| lower.contains(&n.to_ascii_lowercase()))
            })
            .unwrap_or(false)
    })
}

fn writable_parent(path: &str) -> bool {
    let parent = Path::new(path).parent().unwrap_or_else(|| Path::new("/"));
    if parent.exists() {
        fs::metadata(parent)
            .map(|m| !m.permissions().readonly())
            .unwrap_or(false)
    } else {
        // On first boot the directory may be created by the image or daemon.
        true
    }
}

fn is_world_writable(path: &str) -> bool {
    fs::metadata(path)
        .map(|m| (m.permissions().mode() & 0o002) != 0)
        .unwrap_or(false)
}

fn command_available(cmd: &str) -> bool {
    Command::new("sh")
        .arg("-c")
        .arg(format!("command -v {} >/dev/null 2>&1", shell_escape(cmd)))
        .status()
        .map(|s| s.success())
        .unwrap_or(false)
}

fn read_tail(path: &str, max: usize) -> Vec<String> {
    match fs::read_to_string(path) {
        Ok(content) => {
            let lines: Vec<String> = content.lines().rev().take(max).map(str::to_owned).collect();
            lines.into_iter().rev().collect()
        }
        Err(_) => Vec::new(),
    }
}

fn is_incident_line(line: &str) -> bool {
    let l = line.to_ascii_lowercase();
    l.contains("alert")
        || l.contains("intrusion")
        || l.contains("tamper")
        || l.contains("denied")
        || l.contains("failed")
        || l.contains("panic")
}

fn classify_line(line: &str) -> &'static str {
    let l = line.to_ascii_lowercase();
    if l.contains("tamper")
        || l.contains("secure boot")
        || l.contains("panic")
        || l.contains("critical")
    {
        "critical"
    } else if l.contains("intrusion")
        || l.contains("alert")
        || l.contains("denied")
        || l.contains("failed")
    {
        "high"
    } else if l.contains("warning") || l.contains("scan") {
        "medium"
    } else {
        "info"
    }
}

fn shell_escape(s: &str) -> String {
    s.chars()
        .filter(|c| c.is_ascii_alphanumeric() || *c == '-' || *c == '_')
        .collect()
}

fn json_escape(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    for ch in s.chars() {
        match ch {
            '\\' => out.push_str("\\\\"),
            '"' => out.push_str("\\\""),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if c.is_control() => out.push_str(&format!("\\u{:04x}", c as u32)),
            c => out.push(c),
        }
    }
    out
}
