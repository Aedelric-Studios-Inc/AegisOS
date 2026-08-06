//! AegisOS service manager.
//! Reads the service manifest, supervises child processes, and exposes their
//! real runtime state over a Unix socket.
//!
//! Default paths:
//!   config: /etc/services.toml
//!   socket: /run/service-manager.sock
//!
//! Development overrides:
//!   AEGIS_SERVICES_CONFIG
//!   AEGIS_SERVICE_MANAGER_SOCKET

use std::collections::{HashMap, HashSet};
use std::env;
use std::fs;
use std::io::{BufRead, BufReader, Write};
use std::os::unix::fs::PermissionsExt;
use std::os::unix::net::{UnixListener, UnixStream};
use std::process::{Child, Command};
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

const DEFAULT_CONFIG_PATH: &str = "/etc/services.toml";
const DEFAULT_SOCKET_PATH: &str = "/run/service-manager.sock";
const DEFAULT_AEGISD_SOCKET_PATH: &str = "/run/aegisd.sock";
const SIGINT: i32 = 2;
const SIGTERM: i32 = 15;

static SHUTDOWN_REQUESTED: AtomicBool = AtomicBool::new(false);

extern "C" {
    fn signal(signal: i32, handler: usize) -> usize;
}

extern "C" fn request_shutdown(_: i32) {
    SHUTDOWN_REQUESTED.store(true, Ordering::SeqCst);
}

fn install_signal_handlers() {
    unsafe {
        let _ = signal(SIGINT, request_shutdown as usize);
        let _ = signal(SIGTERM, request_shutdown as usize);
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum RestartPolicy {
    Always,
    OnFailure,
    Once,
    Never,
}

impl RestartPolicy {
    fn as_str(self) -> &'static str {
        match self {
            Self::Always => "always",
            Self::OnFailure => "on-failure",
            Self::Once => "once",
            Self::Never => "never",
        }
    }
}

#[derive(Debug, Clone)]
struct ServiceDef {
    name: String,
    binary: String,
    args: Vec<String>,
    env: Vec<(String, String)>,
    inherit_env: Vec<String>,
    restart: RestartPolicy,
    critical: bool,
    requires: Vec<String>,
}

type Children = HashMap<String, Child>;
type ExitCodes = HashMap<String, Option<i32>>;
type RestartCounts = HashMap<String, u64>;

fn main() {
    install_signal_handlers();

    let config_path = env::var("AEGIS_SERVICES_CONFIG")
        .unwrap_or_else(|_| DEFAULT_CONFIG_PATH.to_owned());
    let socket_path = env::var("AEGIS_SERVICE_MANAGER_SOCKET")
        .unwrap_or_else(|_| DEFAULT_SOCKET_PATH.to_owned());

    eprintln!("[service-manager] config: {}", config_path);
    eprintln!("[service-manager] control socket: {}", socket_path);

    let defs = load_services(&config_path);
    if defs.is_empty() {
        eprintln!("[service-manager] no services configured");
    }

    let listener = bind_control_socket(&socket_path);
    supervise(defs, listener, &socket_path);
}

fn bind_control_socket(path: &str) -> UnixListener {
    let _ = fs::remove_file(path);
    let listener = UnixListener::bind(path).unwrap_or_else(|e| {
        eprintln!("[service-manager] bind {}: {}", path, e);
        std::process::exit(1);
    });
    listener.set_nonblocking(true).unwrap_or_else(|e| {
        eprintln!("[service-manager] set_nonblocking {}: {}", path, e);
        std::process::exit(1);
    });
    if let Err(e) = fs::set_permissions(path, fs::Permissions::from_mode(0o660)) {
        eprintln!("[service-manager] chmod {}: {}", path, e);
    }
    listener
}

fn load_services(path: &str) -> Vec<ServiceDef> {
    let content = match fs::read_to_string(path) {
        Ok(c) => c,
        Err(e) => {
            eprintln!("[service-manager] cannot read {}: {}", path, e);
            return vec![];
        }
    };

    let mut defs = Vec::new();
    let mut cur: Option<ServiceDef> = None;

    for raw in content.lines() {
        let line = raw.split('#').next().unwrap_or("").trim();
        if line.is_empty() {
            continue;
        }

        if line == "[[service]]" {
            if let Some(def) = cur.take() {
                push_valid(def, &mut defs);
            }
            cur = Some(ServiceDef {
                name: String::new(),
                binary: String::new(),
                args: Vec::new(),
                env: Vec::new(),
                inherit_env: Vec::new(),
                restart: RestartPolicy::Always,
                critical: false,
                requires: Vec::new(),
            });
            continue;
        }

        let Some(def) = cur.as_mut() else {
            continue;
        };

        if let Some(val) = kv_str(line, "name") {
            def.name = val;
        } else if let Some(val) = kv_str(line, "bin").or_else(|| kv_str(line, "binary")) {
            def.binary = val;
        } else if let Some(args) = kv_str_array(line, "args") {
            def.args = args;
        } else if let Some(entries) = kv_str_array(line, "env") {
            def.env = parse_env_entries(entries, &def.name);
        } else if let Some(entries) = kv_str_array(line, "inherit_env") {
            def.inherit_env = parse_env_names(entries, &def.name);
        } else if let Some(reqs) = kv_str_array(line, "requires") {
            def.requires = reqs;
        } else if let Some(policy) = kv_restart(line, "restart") {
            def.restart = policy;
        } else if let Some(critical) = kv_bool(line, "critical") {
            def.critical = critical;
        }
    }

    if let Some(def) = cur {
        push_valid(def, &mut defs);
    }
    defs
}

fn push_valid(def: ServiceDef, defs: &mut Vec<ServiceDef>) {
    if def.name.is_empty() || def.binary.is_empty() {
        eprintln!("[service-manager] ignoring invalid service stanza: {:?}", def);
        return;
    }
    if defs.iter().any(|existing| existing.name == def.name) {
        eprintln!("[service-manager] ignoring duplicate service: {}", def.name);
        return;
    }
    defs.push(def);
}

fn kv_value<'a>(line: &'a str, key: &str) -> Option<&'a str> {
    let (k, v) = line.split_once('=')?;
    if k.trim() == key {
        Some(v.trim())
    } else {
        None
    }
}

fn kv_str(line: &str, key: &str) -> Option<String> {
    let val = kv_value(line, key)?;
    Some(val.trim_matches('"').to_owned())
}

fn kv_bool(line: &str, key: &str) -> Option<bool> {
    let val = kv_value(line, key)?.trim().trim_matches('"');
    match val {
        "true" => Some(true),
        "false" => Some(false),
        _ => None,
    }
}

fn kv_restart(line: &str, key: &str) -> Option<RestartPolicy> {
    let val = kv_value(line, key)?.trim().trim_matches('"');
    match val {
        "always" | "true" => Some(RestartPolicy::Always),
        "on-failure" => Some(RestartPolicy::OnFailure),
        "once" => Some(RestartPolicy::Once),
        "never" | "false" => Some(RestartPolicy::Never),
        other => {
            eprintln!(
                "[service-manager] unknown restart policy '{}', using never",
                other
            );
            Some(RestartPolicy::Never)
        }
    }
}

fn kv_str_array(line: &str, key: &str) -> Option<Vec<String>> {
    let rest = kv_value(line, key)?;
    if !rest.starts_with('[') || !rest.ends_with(']') {
        return None;
    }
    let inner = &rest[1..rest.len() - 1];
    if inner.trim().is_empty() {
        return Some(Vec::new());
    }
    Some(
        inner
            .split(',')
            .map(|s| s.trim().trim_matches('"').to_owned())
            .filter(|s| !s.is_empty())
            .collect(),
    )
}

fn parse_env_entries(entries: Vec<String>, service_name: &str) -> Vec<(String, String)> {
    entries
        .into_iter()
        .filter_map(|entry| {
            let Some((key, value)) = entry.split_once('=') else {
                eprintln!(
                    "[service-manager] ignoring malformed env entry for {}: {}",
                    service_name, entry
                );
                return None;
            };
            if key.is_empty()
                || !key
                    .bytes()
                    .all(|byte| byte.is_ascii_alphanumeric() || byte == b'_')
            {
                eprintln!(
                    "[service-manager] ignoring invalid env name for {}: {}",
                    service_name, key
                );
                return None;
            }
            Some((key.to_owned(), value.to_owned()))
        })
        .collect()
}

fn parse_env_names(entries: Vec<String>, service_name: &str) -> Vec<String> {
    entries
        .into_iter()
        .filter(|key| {
            let valid = !key.is_empty()
                && key
                    .bytes()
                    .all(|byte| byte.is_ascii_alphanumeric() || byte == b'_');
            if !valid {
                eprintln!(
                    "[service-manager] ignoring invalid inherited env name for {}: {}",
                    service_name, key
                );
            }
            valid
        })
        .collect()
}

fn supervise(defs: Vec<ServiceDef>, listener: UnixListener, socket_path: &str) {
    let mut children: Children = HashMap::new();
    let mut completed: HashSet<String> = HashSet::new();
    let mut manually_stopped: HashSet<String> = HashSet::new();
    let mut restart_counts: RestartCounts = HashMap::new();
    let mut last_exit_codes: ExitCodes = HashMap::new();

    spawn_ready_services(
        &defs,
        &mut children,
        &mut completed,
        &manually_stopped,
        &mut last_exit_codes,
    );

    loop {
        if SHUTDOWN_REQUESTED.load(Ordering::SeqCst) {
            eprintln!("[service-manager] shutdown requested");
            break;
        }

        accept_control_connections(
            &listener,
            &defs,
            &mut children,
            &mut completed,
            &mut manually_stopped,
            &mut restart_counts,
            &mut last_exit_codes,
        );

        for def in &defs {
            let exited = match children.get_mut(&def.name) {
                Some(child) => match child.try_wait() {
                    Ok(Some(status)) => Some(status),
                    Ok(None) => None,
                    Err(e) => {
                        eprintln!("[service-manager] wait {}: {}", def.name, e);
                        None
                    }
                },
                None => None,
            };

            if let Some(status) = exited {
                children.remove(&def.name);
                last_exit_codes.insert(def.name.clone(), status.code());

                if matches!(def.restart, RestartPolicy::Once | RestartPolicy::Never)
                    || (def.restart == RestartPolicy::OnFailure && status.success())
                {
                    completed.insert(def.name.clone());
                }

                let should_restart = !manually_stopped.contains(&def.name)
                    && match def.restart {
                        RestartPolicy::Always => true,
                        RestartPolicy::OnFailure => !status.success(),
                        RestartPolicy::Once | RestartPolicy::Never => false,
                    };

                if should_restart {
                    *restart_counts.entry(def.name.clone()).or_insert(0) += 1;
                    eprintln!(
                        "[service-manager] restarting {} after exit {}",
                        def.name, status
                    );
                    try_spawn(
                        def,
                        &defs,
                        &mut children,
                        &mut completed,
                        &manually_stopped,
                        &mut last_exit_codes,
                    );
                } else {
                    eprintln!(
                        "[service-manager] {} exited {}; not restarting",
                        def.name, status
                    );
                }
            }
        }

        spawn_ready_services(
            &defs,
            &mut children,
            &mut completed,
            &manually_stopped,
            &mut last_exit_codes,
        );

        std::thread::sleep(Duration::from_millis(100));
    }

    shutdown_children(&defs, &mut children);
    let _ = fs::remove_file(socket_path);
    let aegisd_socket = env::var("AEGISD_SOCKET")
        .unwrap_or_else(|_| DEFAULT_AEGISD_SOCKET_PATH.to_owned());
    let _ = fs::remove_file(&aegisd_socket);
    eprintln!("[service-manager] shutdown complete");
}

fn spawn_ready_services(
    defs: &[ServiceDef],
    children: &mut Children,
    completed: &mut HashSet<String>,
    manually_stopped: &HashSet<String>,
    last_exit_codes: &mut ExitCodes,
) {
    for _ in 0..defs.len().max(1) {
        let before = children.len();
        for def in defs {
            if !children.contains_key(&def.name)
                && !completed.contains(&def.name)
                && !manually_stopped.contains(&def.name)
            {
                try_spawn(
                    def,
                    defs,
                    children,
                    completed,
                    manually_stopped,
                    last_exit_codes,
                );
            }
        }
        if children.len() == before {
            break;
        }
    }
}

fn deps_ready(
    def: &ServiceDef,
    children: &Children,
    completed: &HashSet<String>,
) -> bool {
    def.requires
        .iter()
        .all(|name| children.contains_key(name) || completed.contains(name))
}

fn try_spawn(
    def: &ServiceDef,
    defs: &[ServiceDef],
    children: &mut Children,
    completed: &mut HashSet<String>,
    manually_stopped: &HashSet<String>,
    last_exit_codes: &mut ExitCodes,
) {
    if manually_stopped.contains(&def.name) || children.contains_key(&def.name) {
        return;
    }

    for dep in &def.requires {
        if !defs.iter().any(|d| d.name == *dep) {
            eprintln!(
                "[service-manager] {} requires missing service {}; delaying",
                def.name, dep
            );
            return;
        }
    }

    if !deps_ready(def, children, completed) {
        return;
    }

    let mut command = Command::new(&def.binary);
    command.args(&def.args);

    for secret in [
        "AEGIS_DASHBOARD_TOKEN",
        "RUSTMYADMIN_ADMIN_PASSWORD",
        "AEGISD_TOKEN",
    ] {
        command.env_remove(secret);
    }
    for key in &def.inherit_env {
        if let Ok(value) = env::var(key) {
            command.env(key, value);
        }
    }
    for (key, value) in &def.env {
        command.env(key, value);
    }

    match command.spawn() {
        Ok(child) => {
            eprintln!(
                "[service-manager] started {} (pid {})",
                def.name,
                child.id()
            );
            last_exit_codes.remove(&def.name);
            children.insert(def.name.clone(), child);
        }
        Err(e) => {
            eprintln!(
                "[service-manager] failed to start {} at {}: {}",
                def.name, def.binary, e
            );
            last_exit_codes.insert(def.name.clone(), None);
            if matches!(def.restart, RestartPolicy::Once | RestartPolicy::Never) {
                completed.insert(def.name.clone());
            }
            if def.critical {
                eprintln!("[service-manager] critical service {} is unavailable", def.name);
            }
        }
    }
}

fn shutdown_children(defs: &[ServiceDef], children: &mut Children) {
    for def in defs.iter().rev() {
        let Some(mut child) = children.remove(&def.name) else {
            continue;
        };
        eprintln!(
            "[service-manager] stopping {} (pid {})",
            def.name,
            child.id()
        );
        let _ = child.kill();
        let _ = child.wait();
    }
}

fn accept_control_connections(
    listener: &UnixListener,
    defs: &[ServiceDef],
    children: &mut Children,
    completed: &mut HashSet<String>,
    manually_stopped: &mut HashSet<String>,
    restart_counts: &mut RestartCounts,
    last_exit_codes: &mut ExitCodes,
) {
    loop {
        match listener.accept() {
            Ok((mut stream, _)) => {
                if let Err(e) = handle_control_connection(
                    &mut stream,
                    defs,
                    children,
                    completed,
                    manually_stopped,
                    restart_counts,
                    last_exit_codes,
                ) {
                    eprintln!("[service-manager] control request: {}", e);
                }
            }
            Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => break,
            Err(e) => {
                eprintln!("[service-manager] accept: {}", e);
                break;
            }
        }
    }
}

fn handle_control_connection(
    stream: &mut UnixStream,
    defs: &[ServiceDef],
    children: &mut Children,
    completed: &mut HashSet<String>,
    manually_stopped: &mut HashSet<String>,
    restart_counts: &mut RestartCounts,
    last_exit_codes: &mut ExitCodes,
) -> std::io::Result<()> {
    stream.set_read_timeout(Some(Duration::from_secs(2)))?;
    stream.set_write_timeout(Some(Duration::from_secs(2)))?;

    let mut reader = BufReader::new(stream.try_clone()?);
    let mut request_line = String::new();
    reader.read_line(&mut request_line)?;

    loop {
        let mut header = String::new();
        reader.read_line(&mut header)?;
        if header == "\r\n" || header == "\n" || header.is_empty() {
            break;
        }
    }

    let mut parts = request_line.split_whitespace();
    let method = parts.next().unwrap_or("GET");
    let path = parts.next().unwrap_or("/");

    let (status, body) = route_control_request(
        method,
        path,
        defs,
        children,
        completed,
        manually_stopped,
        restart_counts,
        last_exit_codes,
    );

    let reason = match status {
        200 => "OK",
        400 => "Bad Request",
        404 => "Not Found",
        409 => "Conflict",
        _ => "Error",
    };

    write!(
        stream,
        "HTTP/1.0 {} {}\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
        status,
        reason,
        body.len(),
        body
    )?;
    stream.flush()
}

fn route_control_request(
    method: &str,
    path: &str,
    defs: &[ServiceDef],
    children: &mut Children,
    completed: &mut HashSet<String>,
    manually_stopped: &mut HashSet<String>,
    restart_counts: &mut RestartCounts,
    last_exit_codes: &mut ExitCodes,
) -> (u16, String) {
    if method == "GET" && path == "/services" {
        return (
            200,
            list_services_json(
                defs,
                children,
                completed,
                manually_stopped,
                restart_counts,
                last_exit_codes,
            ),
        );
    }

    if method == "POST" && path.starts_with("/services/") {
        let rest = path.trim_start_matches("/services/");
        let Some((name, action)) = rest.rsplit_once('/') else {
            return (400, error_json("invalid service control path"));
        };

        let Some(def) = defs.iter().find(|def| def.name == name) else {
            return (404, error_json("unknown service"));
        };

        return match action {
            "start" => {
                manually_stopped.remove(name);
                completed.remove(name);
                try_spawn(
                    def,
                    defs,
                    children,
                    completed,
                    manually_stopped,
                    last_exit_codes,
                );
                (
                    200,
                    service_json(
                        def,
                        children,
                        completed,
                        manually_stopped,
                        restart_counts,
                        last_exit_codes,
                    ),
                )
            }
            "stop" => {
                stop_service(name, children, completed, manually_stopped, last_exit_codes);
                (
                    200,
                    service_json(
                        def,
                        children,
                        completed,
                        manually_stopped,
                        restart_counts,
                        last_exit_codes,
                    ),
                )
            }
            "restart" => {
                stop_service(name, children, completed, manually_stopped, last_exit_codes);
                manually_stopped.remove(name);
                completed.remove(name);
                *restart_counts.entry(name.to_owned()).or_insert(0) += 1;
                try_spawn(
                    def,
                    defs,
                    children,
                    completed,
                    manually_stopped,
                    last_exit_codes,
                );
                (
                    200,
                    service_json(
                        def,
                        children,
                        completed,
                        manually_stopped,
                        restart_counts,
                        last_exit_codes,
                    ),
                )
            }
            _ => (404, error_json("unknown service action")),
        };
    }

    (404, error_json("not found"))
}

fn stop_service(
    name: &str,
    children: &mut Children,
    completed: &mut HashSet<String>,
    manually_stopped: &mut HashSet<String>,
    last_exit_codes: &mut ExitCodes,
) {
    manually_stopped.insert(name.to_owned());
    completed.remove(name);

    if let Some(mut child) = children.remove(name) {
        if let Err(e) = child.kill() {
            eprintln!("[service-manager] kill {}: {}", name, e);
        }
        match child.wait() {
            Ok(status) => {
                last_exit_codes.insert(name.to_owned(), status.code());
                eprintln!("[service-manager] stopped {} ({})", name, status);
            }
            Err(e) => {
                eprintln!("[service-manager] wait after stop {}: {}", name, e);
                last_exit_codes.insert(name.to_owned(), None);
            }
        }
    }
}

fn list_services_json(
    defs: &[ServiceDef],
    children: &Children,
    completed: &HashSet<String>,
    manually_stopped: &HashSet<String>,
    restart_counts: &RestartCounts,
    last_exit_codes: &ExitCodes,
) -> String {
    let entries = defs
        .iter()
        .map(|def| {
            service_json(
                def,
                children,
                completed,
                manually_stopped,
                restart_counts,
                last_exit_codes,
            )
        })
        .collect::<Vec<_>>()
        .join(",");
    format!(r#"{{"services":[{}]}}"#, entries)
}

fn service_json(
    def: &ServiceDef,
    children: &Children,
    completed: &HashSet<String>,
    manually_stopped: &HashSet<String>,
    restart_counts: &RestartCounts,
    last_exit_codes: &ExitCodes,
) -> String {
    let pid = children
        .get(&def.name)
        .map(|child| child.id().to_string())
        .unwrap_or_else(|| "null".to_owned());

    let state = if children.contains_key(&def.name) {
        "running"
    } else if manually_stopped.contains(&def.name) {
        "stopped"
    } else if completed.contains(&def.name) {
        match last_exit_codes.get(&def.name).copied().flatten() {
            Some(0) => "completed",
            Some(_) => "failed",
            None => "failed",
        }
    } else if matches!(last_exit_codes.get(&def.name), Some(Some(code)) if *code != 0) {
        "failed"
    } else {
        "pending"
    };

    let last_exit_code = last_exit_codes
        .get(&def.name)
        .copied()
        .flatten()
        .map(|code| code.to_string())
        .unwrap_or_else(|| "null".to_owned());

    let requires = def
        .requires
        .iter()
        .map(|item| format!(r#""{}""#, json_escape(item)))
        .collect::<Vec<_>>()
        .join(",");

    format!(
        concat!(
            r#"{{"name":"{}","binary":"{}","state":"{}","active":{},"pid":{},"restart_count":{},"last_exit_code":{},"restart_policy":"{}","critical":{},"requires":[{}]}}"#
        ),
        json_escape(&def.name),
        json_escape(&def.binary),
        state,
        state == "running",
        pid,
        restart_counts.get(&def.name).copied().unwrap_or(0),
        last_exit_code,
        def.restart.as_str(),
        def.critical,
        requires
    )
}

fn error_json(message: &str) -> String {
    format!(r#"{{"error":"{}"}}"#, json_escape(message))
}

fn json_escape(value: &str) -> String {
    let mut out = String::with_capacity(value.len());
    for ch in value.chars() {
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
