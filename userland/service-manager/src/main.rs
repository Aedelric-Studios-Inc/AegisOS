//! AegisOS service manager.
//! Reads /etc/services.toml, spawns each service, and supervises them.

use std::collections::HashMap;
use std::process::{Child, Command};
use std::time::Duration;
use std::fs;

/// A service definition parsed from the config file.
#[derive(Debug, Clone)]
struct ServiceDef {
    name:    String,
    binary:  String,
    args:    Vec<String>,
    restart: bool,
}

fn main() {
    println!("[service-manager] started");
    let defs = load_services("/etc/services.toml");
    if defs.is_empty() {
        eprintln!("[service-manager] no services configured");
    }
    supervise(defs);
}

/// Parse a minimal TOML-like service config.
/// Expected format (each service stanza):
/// ```
/// [[service]]
/// name    = "dns-filter"
/// binary  = "/sbin/dns-filter"
/// args    = ["--config", "/etc/dns-filter.toml"]
/// restart = true
/// ```
fn load_services(path: &str) -> Vec<ServiceDef> {
    let content = match fs::read_to_string(path) {
        Ok(c) => c,
        Err(e) => {
            eprintln!("[service-manager] cannot read {}: {}", path, e);
            return vec![];
        }
    };
    let mut defs  = Vec::new();
    let mut cur: Option<ServiceDef> = None;

    for line in content.lines() {
        let line = line.trim();
        if line == "[[service]]" {
            if let Some(def) = cur.take() { defs.push(def); }
            cur = Some(ServiceDef {
                name:    String::new(),
                binary:  String::new(),
                args:    Vec::new(),
                restart: true,
            });
            continue;
        }
        if let Some(ref mut def) = cur {
            if let Some(val) = kv_str(line, "name")   { def.name   = val; }
            if let Some(val) = kv_str(line, "binary") { def.binary = val; }
            if let Some(val) = kv_bool(line, "restart") { def.restart = val; }
            if let Some(args) = kv_str_array(line, "args") { def.args = args; }
        }
    }
    if let Some(def) = cur { defs.push(def); }
    defs
}

fn kv_str(line: &str, key: &str) -> Option<String> {
    let prefix = format!("{} =", key);
    if line.starts_with(&prefix) {
        let val = line[prefix.len()..].trim().trim_matches('"').to_owned();
        Some(val)
    } else { None }
}

fn kv_bool(line: &str, key: &str) -> Option<bool> {
    kv_str(line, key).and_then(|v| match v.as_str() {
        "true"  => Some(true),
        "false" => Some(false),
        _       => None,
    })
}

fn kv_str_array(line: &str, key: &str) -> Option<Vec<String>> {
    let prefix = format!("{} =", key);
    if !line.starts_with(&prefix) { return None; }
    let rest = line[prefix.len()..].trim();
    if !rest.starts_with('[') || !rest.ends_with(']') { return None; }
    let inner = &rest[1..rest.len()-1];
    Some(inner.split(',').map(|s| s.trim().trim_matches('"').to_owned()).collect())
}

fn supervise(defs: Vec<ServiceDef>) -> ! {
    let mut children: HashMap<String, Child> = HashMap::new();

    // Initial spawn
    for def in &defs {
        spawn_service(def, &mut children);
    }

    loop {
        for def in &defs {
            if !def.restart { continue; }
            let dead = match children.get_mut(&def.name) {
                Some(child) => matches!(child.try_wait(), Ok(Some(_))),
                None => true,
            };
            if dead {
                eprintln!("[service-manager] restarting {}", def.name);
                spawn_service(def, &mut children);
            }
        }
        std::thread::sleep(Duration::from_secs(1));
    }
}

fn spawn_service(def: &ServiceDef, children: &mut HashMap<String, Child>) {
    match Command::new(&def.binary).args(&def.args).spawn() {
        Ok(child) => {
            println!("[service-manager] started {} (pid {})", def.name, child.id());
            children.insert(def.name.clone(), child);
        }
        Err(e) => eprintln!("[service-manager] failed to start {}: {}", def.name, e),
    }
}
