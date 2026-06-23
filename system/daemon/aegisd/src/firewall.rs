//! Firewall/NAT management for aegisd.
//!
//! v5 removes the in-memory-only scaffold and introduces an nftables backend.
//! Runtime rule add/delete still keeps a small daemon-local list for the UI, but
//! the router baseline programs real nftables tables when nft is available.

use std::io::Write;
use std::process::{Command, Stdio};
use std::sync::Mutex;
use crate::network::{self, RouterConfig};

#[derive(Clone)]
pub struct FirewallRule {
    pub id: u32,
    pub chain: String,
    pub protocol: String,
    pub src: String,
    pub dst: String,
    pub port: Option<u16>,
    pub action: String,
}

static RULES: Mutex<Vec<FirewallRule>> = Mutex::new(Vec::new());
static NEXT_ID: std::sync::atomic::AtomicU32 = std::sync::atomic::AtomicU32::new(1);

pub fn status() -> String {
    let nft = network::command_available("nft");
    let ruleset_loaded = nft_ruleset_contains("table inet aegis");
    format!(
        r#"{{"firewall":{{"backend":"nftables","available":{},"baseline_loaded":{},"dry_run":{}}}}}"#,
        nft,
        ruleset_loaded,
        network::dry_run(),
    )
}

pub fn apply_baseline() -> String {
    let cfg = RouterConfig::load();
    let script = nft_baseline(&cfg);
    if network::dry_run() {
        return format!(r#"{{"firewall_apply":{{"ok":true,"backend":"nftables","dry_run":true,"script_bytes":{}}}}}"#, script.len());
    }
    if !network::command_available("nft") {
        return r#"{"firewall_apply":{"ok":false,"backend":"nftables","error":"nft not installed"}}"#.to_owned();
    }
    match run_nft_script(&script) {
        Ok(_) => r#"{"firewall_apply":{"ok":true,"backend":"nftables","baseline":"loaded"}}"#.to_owned(),
        Err(e) => format!(r#"{{"firewall_apply":{{"ok":false,"backend":"nftables","error":"{}"}}}}"#, network::json_escape(&e)),
    }
}

pub fn list_rules() -> String {
    let rules = RULES.lock().unwrap_or_else(|e| e.into_inner());
    let entries: Vec<String> = rules.iter().map(rule_to_json).collect();
    format!(
        r#"{{"backend_status":{},"rules":[{}]}}"#,
        strip_outer(status(), "firewall"),
        entries.join(","),
    )
}

pub fn add_rule(body: &str) -> Result<String, &'static str> {
    let rule = parse_rule(body)?;
    let id = NEXT_ID.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
    let mut r = rule;
    r.id = id;

    let nft_result = apply_runtime_rule(&r);
    let json = rule_to_json(&r);
    RULES.lock().unwrap_or_else(|e| e.into_inner()).push(r);
    Ok(format!(r#"{{"added":{},"nft":{}}}"#, json, nft_result))
}

pub fn delete_rule(id: u32) -> String {
    let mut rules = RULES.lock().unwrap_or_else(|e| e.into_inner());
    let before = rules.len();
    rules.retain(|r| r.id != id);
    let removed = rules.len() < before;
    format!(r#"{{"id":{},"removed":{},"note":"runtime nft handles are not persisted yet; reload baseline to resync"}}"#, id, removed)
}

fn apply_runtime_rule(r: &FirewallRule) -> String {
    if network::dry_run() {
        return r#"{"ok":true,"dry_run":true}"#.to_owned();
    }
    if !network::command_available("nft") {
        return r#"{"ok":false,"error":"nft not installed"}"#.to_owned();
    }
    let chain = match r.chain.as_str() {
        "INPUT" => "input",
        "OUTPUT" => "output",
        "FORWARD" => "forward",
        _ => "input",
    };
    let action = r.action.to_ascii_lowercase();
    let mut args = vec![
        "add".to_owned(),
        "rule".to_owned(),
        "inet".to_owned(),
        "aegis".to_owned(),
        chain.to_owned(),
    ];
    if r.protocol != "any" {
        args.push(r.protocol.clone());
    }
    if let Some(port) = r.port {
        args.push("dport".to_owned());
        args.push(port.to_string());
    }
    args.push(action);
    let ok = Command::new("nft")
        .args(args.iter().map(String::as_str))
        .status()
        .map(|s| s.success())
        .unwrap_or(false);
    format!(r#"{{"ok":{}}}"#, ok)
}

fn nft_baseline(cfg: &RouterConfig) -> String {
    format!(r#"
flush table inet aegis
flush table ip aegis_nat

table inet aegis {{
  chain input {{
    type filter hook input priority 0; policy drop;
    iifname "lo" accept
    ct state established,related accept
    iifname "{lan}" tcp dport {{ 22, 8443 }} accept
    iifname "{lan}" udp dport {{ 53, 67 }} accept
    ip protocol icmp accept
    counter log prefix "AEGIS-DROP-IN " drop
  }}

  chain forward {{
    type filter hook forward priority 0; policy drop;
    ct state established,related accept
    iifname "{lan}" oifname "{wan}" accept
    iifname "{lan}" oifname "{wwan}" accept
    counter log prefix "AEGIS-DROP-FWD " drop
  }}

  chain output {{
    type filter hook output priority 0; policy accept;
  }}
}}

table ip aegis_nat {{
  chain postrouting {{
    type nat hook postrouting priority 100; policy accept;
    oifname "{wan}" masquerade
    oifname "{wwan}" masquerade
  }}
}}
"#, lan=cfg.lan, wan=cfg.wan, wwan=cfg.wwan)
}

fn run_nft_script(script: &str) -> Result<(), String> {
    let mut child = Command::new("nft")
        .arg("-f")
        .arg("-")
        .stdin(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| e.to_string())?;
    child.stdin.as_mut().ok_or("nft stdin unavailable")?
        .write_all(script.as_bytes())
        .map_err(|e| e.to_string())?;
    let out = child.wait_with_output().map_err(|e| e.to_string())?;
    if out.status.success() {
        Ok(())
    } else {
        Err(String::from_utf8_lossy(&out.stderr).trim().to_owned())
    }
}

fn nft_ruleset_contains(needle: &str) -> bool {
    Command::new("nft").args(["list", "ruleset"]).output()
        .map(|out| out.status.success() && String::from_utf8_lossy(&out.stdout).contains(needle))
        .unwrap_or(false)
}

fn rule_to_json(r: &FirewallRule) -> String {
    format!(
        r#"{{"id":{},"chain":"{}","protocol":"{}","src":"{}","dst":"{}","port":{},"action":"{}"}}"#,
        r.id,
        network::json_escape(&r.chain),
        network::json_escape(&r.protocol),
        network::json_escape(&r.src),
        network::json_escape(&r.dst),
        r.port.map(|p| p.to_string()).unwrap_or_else(|| "null".to_owned()),
        network::json_escape(&r.action)
    )
}

/// Minimal JSON field extractor: finds `"key":"value"` or `"key":value`.
fn json_field<'a>(json: &'a str, key: &str) -> Option<&'a str> {
    let needle = format!("\"{}\":", key);
    let start = json.find(&needle)? + needle.len();
    let rest = json[start..].trim_start();
    if let Some(inner) = rest.strip_prefix('"') {
        let end = inner.find('"')?;
        Some(&inner[..end])
    } else {
        let end = rest.find(|c: char| c == ',' || c == '}').unwrap_or(rest.len());
        Some(rest[..end].trim())
    }
}

fn parse_rule(body: &str) -> Result<FirewallRule, &'static str> {
    let chain = json_field(body, "chain").unwrap_or("INPUT");
    let protocol = json_field(body, "protocol").unwrap_or("tcp");
    let action = json_field(body, "action").unwrap_or("DROP");

    if !matches!(chain, "INPUT" | "OUTPUT" | "FORWARD") { return Err("invalid firewall chain"); }
    if !matches!(protocol, "tcp" | "udp" | "icmp" | "any") { return Err("invalid firewall protocol"); }
    if !matches!(action, "ACCEPT" | "DROP" | "REJECT") { return Err("invalid firewall action"); }

    Ok(FirewallRule {
        id: 0,
        chain: chain.to_owned(),
        protocol: protocol.to_owned(),
        src: json_field(body, "src").unwrap_or("any").to_owned(),
        dst: json_field(body, "dst").unwrap_or("any").to_owned(),
        port: json_field(body, "port").and_then(|v| v.parse().ok()),
        action: action.to_owned(),
    })
}

fn strip_outer(json: String, key: &str) -> String {
    let prefix = format!(r#"{{"{}":"#, key);
    if let Some(rest) = json.strip_prefix(&prefix) {
        if let Some(inner) = rest.strip_suffix('}') {
            return inner.to_owned();
        }
    }
    json
}
