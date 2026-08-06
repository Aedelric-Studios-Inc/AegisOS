//! Shared application state.

use std::collections::BTreeMap;
use std::net::{IpAddr, Ipv4Addr, Ipv6Addr};

use crate::cpu_accel::{ad_block_score, dns_filter_score, static_ip_score, vpn_score};

pub struct AppState {
    pub version: &'static str,
    hostname: &'static str,
    services: Vec<ServiceStatus>,
    devices: Vec<DeviceRecord>,
    events: Vec<SecurityEvent>,
    admin_databases: Vec<AdminDatabase>,
    overview: Overview,
}

#[derive(Clone, Copy)]
pub enum RiskLevel {
    Trusted,
    Monitor,
    Blocked,
}

impl RiskLevel {
    pub fn label(self) -> &'static str {
        match self {
            Self::Trusted => "Trusted",
            Self::Monitor => "Monitor",
            Self::Blocked => "Blocked",
        }
    }

    pub fn class_name(self) -> &'static str {
        match self {
            Self::Trusted => "good",
            Self::Monitor => "warn",
            Self::Blocked => "bad",
        }
    }
}

#[derive(Clone, Copy)]
pub enum Severity {
    Info,
    Warning,
    Critical,
}

impl Severity {
    pub fn label(self) -> &'static str {
        match self {
            Self::Info => "Info",
            Self::Warning => "Warning",
            Self::Critical => "Critical",
        }
    }

    pub fn class_name(self) -> &'static str {
        match self {
            Self::Info => "good",
            Self::Warning => "warn",
            Self::Critical => "bad",
        }
    }
}

pub struct DeviceRecord {
    pub hostname: &'static str,
    pub ip: &'static str,
    pub mac: &'static str,
    pub roles: Vec<&'static str>,
    pub open_ports: Vec<u16>,
    pub risk: RiskLevel,
    pub reason: String,
    pub enforcement: String,
}

pub struct ServiceStatus {
    pub slug: &'static str,
    pub name: &'static str,
    pub category: &'static str,
    pub endpoint: &'static str,
    pub status: &'static str,
    pub summary: String,
    pub health_score: u32,
}

impl ServiceStatus {
    pub fn status_class(&self) -> &'static str {
        if self.health_score >= 85 {
            "good"
        } else if self.health_score >= 60 {
            "warn"
        } else {
            "bad"
        }
    }
}

pub struct SecurityEvent {
    pub timestamp: &'static str,
    pub severity: Severity,
    pub message: String,
}

pub struct AdminDatabase {
    pub slug: &'static str,
    pub name: &'static str,
    pub engine: &'static str,
    pub access: &'static str,
    pub summary: String,
    pub tables: Vec<AdminTable>,
}

pub struct AdminTable {
    pub slug: &'static str,
    pub name: &'static str,
    pub engine: &'static str,
    pub rows: usize,
    pub note: String,
    pub recommended_query: &'static str,
    pub columns: Vec<AdminColumn>,
    pub sample_rows: Vec<Vec<String>>,
}

pub struct AdminColumn {
    pub name: &'static str,
    pub data_type: &'static str,
    pub nullable: bool,
    pub key: &'static str,
}

pub struct Overview {
    pub total_devices: usize,
    pub blocked_devices: usize,
    pub monitored_devices: usize,
    pub online_services: usize,
    pub network_risk: RiskLevel,
    pub summary: String,
    pub action_summary: String,
}

impl AppState {
    pub fn new() -> Self {
        let devices = build_devices();
        let services = build_services(&devices);
        let events = build_events(&devices, &services);
        let admin_databases = build_admin_databases(&devices, &services, &events);
        let overview = build_overview(&devices, &services);

        Self {
            version: env!("CARGO_PKG_VERSION"),
            hostname: "aegisbox",
            services,
            devices,
            events,
            admin_databases,
            overview,
        }
    }

    pub fn overview(&self) -> &Overview {
        &self.overview
    }

    pub fn services(&self) -> &[ServiceStatus] {
        &self.services
    }

    pub fn devices(&self) -> &[DeviceRecord] {
        &self.devices
    }

    pub fn events(&self) -> &[SecurityEvent] {
        &self.events
    }

    pub fn admin_databases(&self) -> &[AdminDatabase] {
        &self.admin_databases
    }

    pub fn find_admin_database(&self, slug: &str) -> Option<&AdminDatabase> {
        self.admin_databases.iter().find(|database| database.slug == slug)
    }

    pub fn find_admin_table(
        &self,
        database_slug: &str,
        table_slug: &str,
    ) -> Option<(&AdminDatabase, &AdminTable)> {
        let database = self.find_admin_database(database_slug)?;
        let table = database.tables.iter().find(|table| table.slug == table_slug)?;
        Some((database, table))
    }

    pub fn status_json(&self) -> String {
        let services = self
            .services
            .iter()
            .map(|service| {
                format!(
                    "{{\"slug\":\"{}\",\"name\":\"{}\",\"category\":\"{}\",\"status\":\"{}\",\"score\":{},\"summary\":\"{}\",\"endpoint\":\"{}\"}}",
                    service.slug,
                    service.name,
                    service.category,
                    service.status,
                    service.health_score,
                    json_escape(&service.summary),
                    json_escape(service.endpoint)
                )
            })
            .collect::<Vec<_>>()
            .join(",");

        format!(
            "{{\"status\":\"ok\",\"hostname\":\"{}\",\"version\":\"{}\",\"devices\":{},\"blocked\":{},\"monitored\":{},\"services\":[{}]}}",
            self.hostname,
            self.version,
            self.overview.total_devices,
            self.overview.blocked_devices,
            self.overview.monitored_devices,
            services
        )
    }

    pub fn devices_json(&self) -> String {
        let devices = self
            .devices
            .iter()
            .map(|device| {
                let ports = device
                    .open_ports
                    .iter()
                    .map(u16::to_string)
                    .collect::<Vec<_>>()
                    .join(",");
                let roles = device
                    .roles
                    .iter()
                    .map(|role| format!("\"{}\"", json_escape(role)))
                    .collect::<Vec<_>>()
                    .join(",");

                format!(
                    "{{\"hostname\":\"{}\",\"ip\":\"{}\",\"mac\":\"{}\",\"risk\":\"{}\",\"reason\":\"{}\",\"enforcement\":\"{}\",\"ports\":[{}],\"roles\":[{}]}}",
                    json_escape(device.hostname),
                    json_escape(device.ip),
                    json_escape(device.mac),
                    device.risk.label(),
                    json_escape(&device.reason),
                    json_escape(&device.enforcement),
                    ports,
                    roles
                )
            })
            .collect::<Vec<_>>()
            .join(",");

        format!("{{\"devices\":[{}]}}", devices)
    }

    pub fn rustmyadmin_catalog_json(&self) -> String {
        let databases = self
            .admin_databases
            .iter()
            .map(|database| {
                let tables = database
                    .tables
                    .iter()
                    .map(|table| {
                        format!(
                            "{{\"slug\":\"{}\",\"name\":\"{}\",\"engine\":\"{}\",\"rows\":{},\"columns\":{}}}",
                            json_escape(table.slug),
                            json_escape(table.name),
                            json_escape(table.engine),
                            table.rows,
                            table.columns.len()
                        )
                    })
                    .collect::<Vec<_>>()
                    .join(",");

                format!(
                    "{{\"slug\":\"{}\",\"name\":\"{}\",\"engine\":\"{}\",\"access\":\"{}\",\"summary\":\"{}\",\"tables\":[{}]}}",
                    json_escape(database.slug),
                    json_escape(database.name),
                    json_escape(database.engine),
                    json_escape(database.access),
                    json_escape(&database.summary),
                    tables
                )
            })
            .collect::<Vec<_>>()
            .join(",");

        format!(
            "{{\"readonly\":true,\"databases\":[{}],\"hostname\":\"{}\"}}",
            databases,
            json_escape(self.hostname)
        )
    }

    pub fn rustmyadmin_table_json(&self, database_slug: &str, table_slug: &str) -> Option<String> {
        let (database, table) = self.find_admin_table(database_slug, table_slug)?;
        let columns = table
            .columns
            .iter()
            .map(|column| {
                format!(
                    "{{\"name\":\"{}\",\"type\":\"{}\",\"nullable\":{},\"key\":\"{}\"}}",
                    json_escape(column.name),
                    json_escape(column.data_type),
                    column.nullable,
                    json_escape(column.key)
                )
            })
            .collect::<Vec<_>>()
            .join(",");
        let sample_rows = table
            .sample_rows
            .iter()
            .map(|row| {
                let values = row
                    .iter()
                    .map(|value| format!("\"{}\"", json_escape(value)))
                    .collect::<Vec<_>>()
                    .join(",");
                format!("[{}]", values)
            })
            .collect::<Vec<_>>()
            .join(",");

        Some(format!(
            "{{\"database\":\"{}\",\"table\":\"{}\",\"engine\":\"{}\",\"rows\":{},\"note\":\"{}\",\"recommended_query\":\"{}\",\"columns\":[{}],\"sample_rows\":[{}]}}",
            json_escape(database.name),
            json_escape(table.name),
            json_escape(table.engine),
            table.rows,
            json_escape(&table.note),
            json_escape(table.recommended_query),
            columns,
            sample_rows
        ))
    }
}

#[derive(Clone, Copy)]
struct DeviceSeed {
    hostname: &'static str,
    ip: &'static str,
    mac: &'static str,
    roles: &'static [&'static str],
    open_ports: &'static [u16],
}

fn build_devices() -> Vec<DeviceRecord> {
    let seeds = [
        DeviceSeed {
            hostname: "aegisbox-core",
            ip: "192.168.1.1",
            mac: "02:42:ac:11:00:01",
            roles: &["gateway", "dns", "vpn", "nginx"],
            open_ports: &[53, 80, 443, 51820],
        },
        DeviceSeed {
            hostname: "build-node",
            ip: "192.168.1.10",
            mac: "02:42:ac:11:00:10",
            roles: &["docker", "ci"],
            open_ports: &[22, 8080],
        },
        DeviceSeed {
            hostname: "media-nas",
            ip: "192.168.1.20",
            mac: "02:42:ac:11:00:20",
            roles: &["hosting", "storage"],
            open_ports: &[80, 443, 2049],
        },
        DeviceSeed {
            hostname: "guest-phone",
            ip: "192.168.1.77",
            mac: "02:42:ac:11:00:77",
            roles: &["guest"],
            open_ports: &[],
        },
        DeviceSeed {
            hostname: "rogue-proxy",
            ip: "198.51.100.24",
            mac: "02:42:ac:11:10:24",
            roles: &["unknown", "proxy"],
            open_ports: &[3128, 8080],
        },
        DeviceSeed {
            hostname: "duplicate-printer",
            ip: "192.168.1.20",
            mac: "02:42:ac:11:00:2a",
            roles: &["printer"],
            open_ports: &[9100],
        },
    ];

    let mut counts = BTreeMap::new();
    for seed in &seeds {
        *counts.entry(seed.ip).or_insert(0_u32) += 1;
    }

    seeds
        .into_iter()
        .map(|seed| classify_device(seed, counts.get(seed.ip).copied().unwrap_or(1) > 1))
        .collect()
}

fn classify_device(seed: DeviceSeed, duplicate_ip: bool) -> DeviceRecord {
    let parsed = seed.ip.parse::<IpAddr>().ok();
    let private = parsed.is_some_and(is_private_network_ip);
    let invalid_scope = parsed.is_some_and(is_invalid_for_lan);
    let exposed_proxy = seed
        .open_ports
        .iter()
        .any(|port| matches!(*port, 3128 | 8080 | 9100));
    let risk = if invalid_scope || duplicate_ip {
        RiskLevel::Blocked
    } else if !private || exposed_proxy || seed.roles.contains(&"guest") {
        RiskLevel::Monitor
    } else {
        RiskLevel::Trusted
    };

    let reason = if invalid_scope {
        format!(
            "{} advertises a documentation or public address and should not join the protected LAN.",
            seed.ip
        )
    } else if duplicate_ip {
        format!("Duplicate static IP {} detected on the segment.", seed.ip)
    } else if seed.roles.contains(&"guest") {
        "Guest device is isolated until DNS and ad-block policy is confirmed.".to_owned()
    } else if exposed_proxy {
        "Open proxy or privileged service requires continuous monitoring.".to_owned()
    } else {
        "Address is private, unique, and matched to an expected role.".to_owned()
    };

    let enforcement = match risk {
        RiskLevel::Trusted => "Allow + inspect DNS logs".to_owned(),
        RiskLevel::Monitor => "Monitor + apply stricter filtering".to_owned(),
        RiskLevel::Blocked => "Quarantine + disable upstream access".to_owned(),
    };

    DeviceRecord {
        hostname: seed.hostname,
        ip: seed.ip,
        mac: seed.mac,
        roles: seed.roles.to_vec(),
        open_ports: seed.open_ports.to_vec(),
        risk,
        reason,
        enforcement,
    }
}

fn build_services(devices: &[DeviceRecord]) -> Vec<ServiceStatus> {
    let blocked = devices
        .iter()
        .filter(|device| matches!(device.risk, RiskLevel::Blocked))
        .count() as u32;
    let monitored = devices
        .iter()
        .filter(|device| matches!(device.risk, RiskLevel::Monitor))
        .count() as u32;
    let reserved = devices
        .iter()
        .filter(|device| device.ip.starts_with("192.168.1."))
        .count() as u32;

    vec![
        ServiceStatus {
            slug: "dns-filter",
            name: "DNS Filtering",
            category: "Resolver and sinkhole",
            endpoint: "UDP/TCP 53 · blocklists + malware sinkhole",
            status: "Intercepting risky domains before clients leave the LAN.",
            summary: format!(
                "{} of {} observed clients are behind the filtering chain.",
                devices.len().saturating_sub(blocked as usize),
                devices.len()
            ),
            health_score: dns_filter_score(devices.len() as u32 - blocked, devices.len() as u32),
        },
        ServiceStatus {
            slug: "vpn",
            name: "VPN Gateway",
            category: "WireGuard-compatible access",
            endpoint: "UDP 51820 · peer handshakes monitored",
            status: "Remote peers must complete fresh handshakes before reaching hosted services.",
            summary: format!(
                "{} trusted peers active, {} peer requires review.",
                devices
                    .iter()
                    .filter(|device| matches!(device.risk, RiskLevel::Trusted))
                    .count(),
                monitored
            ),
            health_score: vpn_score(4, 3),
        },
        ServiceStatus {
            slug: "static-ip",
            name: "Static IP Control",
            category: "Lease validation",
            endpoint: "LAN lease reservations + duplicate detection",
            status: "Duplicate addresses are downgraded before they can route traffic.",
            summary: format!(
                "{} reserved infrastructure leases with {} collision detected.",
                reserved, blocked
            ),
            health_score: static_ip_score(reserved, blocked),
        },
        ServiceStatus {
            slug: "ad-blocker",
            name: "Ad Blocker",
            category: "Privacy filtering",
            endpoint: "Embedded lists + domain suppression",
            status: "Advertising and telemetry domains are removed before content is served.",
            summary: "AegisOS keeps noisy endpoints away from guest and production devices."
                .to_owned(),
            health_score: ad_block_score(87, 100),
        },
        ServiceStatus {
            slug: "cloudflare-tunnel",
            name: "Cloudflared Tunnel",
            category: "Zero-trust ingress",
            endpoint: "Outbound tunnel pinned to home.example.com",
            status: "Tunnel exposure is limited to the reverse proxy and verified hostnames.",
            summary: "Only the AegisBox proxy tier is allowed to advertise external ingress."
                .to_owned(),
            health_score: 93,
        },
        ServiceStatus {
            slug: "nginx",
            name: "Nginx Reverse Proxy",
            category: "North-south traffic gateway",
            endpoint: "HTTP/S 80,443 · TLS termination",
            status: "Front-door traffic is consolidated behind a single inspection point.",
            summary: "Public workloads sit behind DNS, VPN, and IP validation checks.".to_owned(),
            health_score: 91,
        },
        ServiceStatus {
            slug: "docker",
            name: "Docker Runtime",
            category: "Container workload isolation",
            endpoint: "Local bridge + bounded service ports",
            status:
                "Containers are visible so exposed ports can be compared against client IP posture.",
            summary:
                "Application hosts are separated from gateway services to reduce blast radius."
                    .to_owned(),
            health_score: 89,
        },
        ServiceStatus {
            slug: "server-hosting",
            name: "Server Hosting",
            category: "Hosted applications",
            endpoint: "NAS and internal apps behind the proxy",
            status: "Hosted services inherit the same filtering and tunnel visibility model.",
            summary: "User-facing apps only remain online when their source IPs stay compliant."
                .to_owned(),
            health_score: 90,
        },
    ]
}

fn build_events(devices: &[DeviceRecord], services: &[ServiceStatus]) -> Vec<SecurityEvent> {
    let highest_service = services
        .iter()
        .max_by_key(|service| service.health_score)
        .map(|service| service.name)
        .unwrap_or("Platform");
    let blocked_devices = devices
        .iter()
        .filter(|device| matches!(device.risk, RiskLevel::Blocked))
        .map(|device| device.hostname)
        .collect::<Vec<_>>();

    vec![
        SecurityEvent {
            timestamp: "2026-06-19T14:15:02Z",
            severity: Severity::Critical,
            message: format!(
                "Quarantined {} after it presented 198.51.100.24 on the internal segment.",
                blocked_devices.first().copied().unwrap_or("unknown-client")
            ),
        },
        SecurityEvent {
            timestamp: "2026-06-19T14:22:47Z",
            severity: Severity::Warning,
            message: "Duplicate static lease 192.168.1.20 forced into restricted mode.".to_owned(),
        },
        SecurityEvent {
            timestamp: "2026-06-19T14:31:11Z",
            severity: Severity::Info,
            message: format!("{highest_service} currently has the strongest health score."),
        },
        SecurityEvent {
            timestamp: "2026-06-19T14:45:09Z",
            severity: Severity::Info,
            message: "Cloudflared, Nginx, Docker, and hosted apps remain tied to the protected ingress path.".to_owned(),
        },
    ]
}

fn build_admin_databases(
    devices: &[DeviceRecord],
    services: &[ServiceStatus],
    events: &[SecurityEvent],
) -> Vec<AdminDatabase> {
    vec![
        AdminDatabase {
            slug: "control-plane",
            name: "control_plane",
            engine: "AegisOS/heap",
            access: "owner-readonly",
            summary: "Dashboard inventory adapted into a RustMyAdmin-style schema browser."
                .to_owned(),
            tables: vec![
                AdminTable {
                    slug: "devices",
                    name: "devices",
                    engine: "virtual",
                    rows: devices.len(),
                    note: "Observed clients with policy decisions, roles, and exposed ports."
                        .to_owned(),
                    recommended_query:
                        "SELECT hostname, ip, risk FROM devices ORDER BY hostname LIMIT 25;",
                    columns: vec![
                        AdminColumn {
                            name: "hostname",
                            data_type: "TEXT",
                            nullable: false,
                            key: "PRI",
                        },
                        AdminColumn {
                            name: "ip",
                            data_type: "TEXT",
                            nullable: false,
                            key: "UNI",
                        },
                        AdminColumn {
                            name: "mac",
                            data_type: "TEXT",
                            nullable: false,
                            key: "",
                        },
                        AdminColumn {
                            name: "risk",
                            data_type: "TEXT",
                            nullable: false,
                            key: "MUL",
                        },
                        AdminColumn {
                            name: "roles",
                            data_type: "TEXT",
                            nullable: false,
                            key: "",
                        },
                        AdminColumn {
                            name: "enforcement",
                            data_type: "TEXT",
                            nullable: false,
                            key: "",
                        },
                    ],
                    sample_rows: devices
                        .iter()
                        .take(4)
                        .map(|device| {
                            vec![
                                device.hostname.to_owned(),
                                device.ip.to_owned(),
                                device.mac.to_owned(),
                                device.risk.label().to_owned(),
                                device.roles.join(", "),
                                device.enforcement.clone(),
                            ]
                        })
                        .collect(),
                },
                AdminTable {
                    slug: "services",
                    name: "services",
                    engine: "virtual",
                    rows: services.len(),
                    note: "Service health scores and exposure summaries for protected workloads."
                        .to_owned(),
                    recommended_query:
                        "SELECT name, category, health_score FROM services ORDER BY health_score DESC;",
                    columns: vec![
                        AdminColumn {
                            name: "name",
                            data_type: "TEXT",
                            nullable: false,
                            key: "PRI",
                        },
                        AdminColumn {
                            name: "category",
                            data_type: "TEXT",
                            nullable: false,
                            key: "",
                        },
                        AdminColumn {
                            name: "endpoint",
                            data_type: "TEXT",
                            nullable: false,
                            key: "",
                        },
                        AdminColumn {
                            name: "status",
                            data_type: "TEXT",
                            nullable: false,
                            key: "",
                        },
                        AdminColumn {
                            name: "health_score",
                            data_type: "INTEGER",
                            nullable: false,
                            key: "MUL",
                        },
                    ],
                    sample_rows: services
                        .iter()
                        .take(4)
                        .map(|service| {
                            vec![
                                service.name.to_owned(),
                                service.category.to_owned(),
                                service.endpoint.to_owned(),
                                service.status.to_owned(),
                                service.health_score.to_string(),
                            ]
                        })
                        .collect(),
                },
            ],
        },
        AdminDatabase {
            slug: "security",
            name: "security",
            engine: "AegisOS/audit",
            access: "owner-readonly",
            summary: "Audit-oriented tables that mirror the event log and quarantine decisions."
                .to_owned(),
            tables: vec![AdminTable {
                slug: "events",
                name: "events",
                engine: "append-only",
                rows: events.len(),
                note: "Recent policy actions and operator-facing telemetry.".to_owned(),
                recommended_query:
                    "SELECT timestamp, severity, message FROM events ORDER BY timestamp DESC LIMIT 20;",
                columns: vec![
                    AdminColumn {
                        name: "timestamp",
                        data_type: "TEXT",
                        nullable: false,
                        key: "PRI",
                    },
                    AdminColumn {
                        name: "severity",
                        data_type: "TEXT",
                        nullable: false,
                        key: "MUL",
                    },
                    AdminColumn {
                        name: "message",
                        data_type: "TEXT",
                        nullable: false,
                        key: "",
                    },
                ],
                sample_rows: events
                    .iter()
                    .take(4)
                    .map(|event| {
                        vec![
                            event.timestamp.to_owned(),
                            event.severity.label().to_owned(),
                            event.message.clone(),
                        ]
                    })
                    .collect(),
            }],
        },
        AdminDatabase {
            slug: "content",
            name: "content",
            engine: "AegisOS/dashboard",
            access: "owner-readonly",
            summary: "Content-facing metadata that lets operators inspect the dashboard posture."
                .to_owned(),
            tables: vec![AdminTable {
                slug: "overview",
                name: "overview",
                engine: "materialized",
                rows: 1,
                note: "Single-row posture summary suitable for health panels or exports."
                    .to_owned(),
                recommended_query:
                    "SELECT total_devices, blocked_devices, monitored_devices, online_services FROM overview;",
                columns: vec![
                    AdminColumn {
                        name: "total_devices",
                        data_type: "INTEGER",
                        nullable: false,
                        key: "",
                    },
                    AdminColumn {
                        name: "blocked_devices",
                        data_type: "INTEGER",
                        nullable: false,
                        key: "",
                    },
                    AdminColumn {
                        name: "monitored_devices",
                        data_type: "INTEGER",
                        nullable: false,
                        key: "",
                    },
                    AdminColumn {
                        name: "online_services",
                        data_type: "INTEGER",
                        nullable: false,
                        key: "",
                    },
                    AdminColumn {
                        name: "summary",
                        data_type: "TEXT",
                        nullable: false,
                        key: "",
                    },
                ],
                sample_rows: vec![vec![
                    devices.len().to_string(),
                    devices
                        .iter()
                        .filter(|device| matches!(device.risk, RiskLevel::Blocked))
                        .count()
                        .to_string(),
                    devices
                        .iter()
                        .filter(|device| matches!(device.risk, RiskLevel::Monitor))
                        .count()
                        .to_string(),
                    services
                        .iter()
                        .filter(|service| service.health_score >= 60)
                        .count()
                        .to_string(),
                    format!(
                        "{} databases mirrored into a RustMyAdmin-style inspector.",
                        3
                    ),
                ]],
            }],
        },
    ]
}

fn build_overview(devices: &[DeviceRecord], services: &[ServiceStatus]) -> Overview {
    let blocked = devices
        .iter()
        .filter(|device| matches!(device.risk, RiskLevel::Blocked))
        .count();
    let monitored = devices
        .iter()
        .filter(|device| matches!(device.risk, RiskLevel::Monitor))
        .count();
    let online_services = services
        .iter()
        .filter(|service| service.health_score >= 60)
        .count();
    let network_risk = if blocked > 0 {
        RiskLevel::Blocked
    } else if monitored > 0 {
        RiskLevel::Monitor
    } else {
        RiskLevel::Trusted
    };

    Overview {
        total_devices: devices.len(),
        blocked_devices: blocked,
        monitored_devices: monitored,
        online_services,
        network_risk,
        summary: format!(
            "{} clients are trusted, {} are under review, and {} are blocked pending operator action.",
            devices
                .iter()
                .filter(|device| matches!(device.risk, RiskLevel::Trusted))
                .count(),
            monitored,
            blocked
        ),
        action_summary: "Review duplicate static leases and keep public-facing services behind the proxy tier.".to_owned(),
    }
}

fn is_private_network_ip(ip: IpAddr) -> bool {
    match ip {
        IpAddr::V4(ipv4) => is_private_ipv4(ipv4),
        IpAddr::V6(ipv6) => is_private_ipv6(ipv6),
    }
}

fn is_invalid_for_lan(ip: IpAddr) -> bool {
    match ip {
        IpAddr::V4(ipv4) => {
            is_documentation_ipv4(ipv4) || ipv4.is_loopback() || ipv4.is_broadcast()
        }
        IpAddr::V6(ipv6) => ipv6.is_loopback() || ipv6.is_unspecified(),
    }
}

fn is_private_ipv4(ip: Ipv4Addr) -> bool {
    let [a, b, _, _] = ip.octets();
    matches!((a, b), (10, _) | (172, 16..=31) | (192, 168))
}

fn is_private_ipv6(ip: Ipv6Addr) -> bool {
    (ip.segments()[0] & 0xfe00) == 0xfc00
}

fn is_documentation_ipv4(ip: Ipv4Addr) -> bool {
    let [a, b, c, _] = ip.octets();
    matches!((a, b, c), (192, 0, 2) | (198, 51, 100) | (203, 0, 113))
}

fn json_escape(value: &str) -> String {
    let mut escaped = String::with_capacity(value.len());
    for ch in value.chars() {
        match ch {
            '"' => escaped.push_str("\\\""),
            '\\' => escaped.push_str("\\\\"),
            '\n' => escaped.push_str("\\n"),
            '\r' => escaped.push_str("\\r"),
            '\t' => escaped.push_str("\\t"),
            _ => escaped.push(ch),
        }
    }
    escaped
}

#[cfg(test)]
mod tests {
    use super::{AppState, RiskLevel};

    #[test]
    fn flags_duplicate_and_public_addresses() {
        let state = AppState::new();
        let blocked = state
            .devices()
            .iter()
            .filter(|device| matches!(device.risk, RiskLevel::Blocked))
            .count();
        assert!(blocked >= 2);
        assert!(state
            .devices()
            .iter()
            .any(|device| device.hostname == "rogue-proxy"
                && matches!(device.risk, RiskLevel::Blocked)));
    }

    #[test]
    fn emits_json_for_dashboard_status() {
        let state = AppState::new();
        let json = state.status_json();
        assert!(json.contains("\"status\":\"ok\""));
        assert!(json.contains("\"services\""));
        assert!(json.contains("\"hostname\":\"aegisbox\""));
    }

    #[test]
    fn builds_rustmyadmin_catalog() {
        let state = AppState::new();
        let json = state.rustmyadmin_catalog_json();
        assert!(state.find_admin_database("control-plane").is_some());
        assert!(json.contains("\"readonly\":true"));
        assert!(json.contains("\"control-plane\""));
    }

    #[test]
    fn emits_table_json_for_rustmyadmin_views() {
        let state = AppState::new();
        let json = state
            .rustmyadmin_table_json("security", "events")
            .expect("security/events table should exist");
        assert!(json.contains("\"table\":\"events\""));
        assert!(json.contains("\"sample_rows\""));
    }
}
