//! Firewall rule management for aegisd.

use std::sync::Mutex;

#[derive(Clone)]
pub struct FirewallRule {
    pub id:       u32,
    pub chain:    String,
    pub protocol: String,
    pub src:      String,
    pub dst:      String,
    pub port:     Option<u16>,
    pub action:   String,
}

static RULES: Mutex<Vec<FirewallRule>> = Mutex::new(Vec::new());
static NEXT_ID: std::sync::atomic::AtomicU32 = std::sync::atomic::AtomicU32::new(1);

pub fn list_rules() -> String {
    let rules = RULES.lock().unwrap_or_else(|e| e.into_inner());
    let entries: Vec<String> = rules.iter().map(rule_to_json).collect();
    format!(r#"{{"rules":[{}]}}"#, entries.join(","))
}

pub fn add_rule(body: &str) -> Result<String, &'static str> {
    let rule = parse_rule(body)?;
    let id = NEXT_ID.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
    let mut r = rule;
    r.id = id;
    let json = rule_to_json(&r);
    RULES.lock().unwrap_or_else(|e| e.into_inner()).push(r);
    Ok(format!(r#"{{"added":{}}}"#, json))
}

pub fn delete_rule(id: u32) -> String {
    let mut rules = RULES.lock().unwrap_or_else(|e| e.into_inner());
    let before = rules.len();
    rules.retain(|r| r.id != id);
    let removed = rules.len() < before;
    format!(r#"{{"id":{},"removed":{}}}"#, id, removed)
}

fn rule_to_json(r: &FirewallRule) -> String {
    format!(
        r#"{{"id":{},"chain":"{}","protocol":"{}","src":"{}","dst":"{}","port":{},"action":"{}"}}"#,
        r.id,
        r.chain,
        r.protocol,
        r.src,
        r.dst,
        r.port.map(|p| p.to_string()).unwrap_or_else(|| "null".to_owned()),
        r.action
    )
}

/// Minimal JSON field extractor: finds `"key":"value"` or `"key":value`.
fn json_field<'a>(json: &'a str, key: &str) -> Option<&'a str> {
    let needle = format!("\"{}\":", key);
    let start  = json.find(&needle)? + needle.len();
    let rest   = json[start..].trim_start();
    if rest.starts_with('"') {
        let inner = &rest[1..];
        let end = inner.find('"')?;
        Some(&inner[..end])
    } else {
        let end = rest.find(|c: char| c == ',' || c == '}').unwrap_or(rest.len());
        Some(rest[..end].trim())
    }
}

fn parse_rule(body: &str) -> Result<FirewallRule, &'static str> {
    Ok(FirewallRule {
        id:       0,
        chain:    json_field(body, "chain")   .unwrap_or("INPUT") .to_owned(),
        protocol: json_field(body, "protocol").unwrap_or("tcp")   .to_owned(),
        src:      json_field(body, "src")     .unwrap_or("any")   .to_owned(),
        dst:      json_field(body, "dst")     .unwrap_or("any")   .to_owned(),
        port:     json_field(body, "port")
                    .and_then(|v| v.parse().ok()),
        action:   json_field(body, "action")  .unwrap_or("DROP")  .to_owned(),
    })
}
