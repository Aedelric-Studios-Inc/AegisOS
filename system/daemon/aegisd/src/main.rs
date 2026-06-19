//! aegisd — AegisOS system daemon.
//!
//! Provides a Unix-socket JSON API consumed by the dashboard, rustmyadmin,
//! and aegisctl.  The socket lives at `/run/aegisd.sock`.
//!
//! Protocol: line-framed HTTP/1.0-style requests and JSON responses.
//! Each connection handles one request then closes.
//!
//! Routes (all read-only unless noted):
//!   GET  /health                    — daemon liveness + uptime
//!   GET  /services                  — list all managed services and their state
//!   POST /services/{name}/start     — start a named service
//!   POST /services/{name}/stop      — stop a named service
//!   GET  /firewall/rules            — list active firewall rules
//!   POST /firewall/rules            — add a firewall rule (body: JSON rule)
//!   DELETE /firewall/rules/{id}     — remove a firewall rule by id
//!   GET  /logs                      — recent kernel/service log lines
//!   GET  /vpn/status                — VPN tunnel status
//!   GET  /updates                   — pending update list
//!   GET  /containers                — running containers
//!   GET  /backups                   — available backups

mod api;
mod auth;
mod backups;
mod containers;
mod firewall;
mod health;
mod logs;
mod services;
mod updates;
mod vpn;

use std::io::{BufRead, BufReader, Write};
use std::os::unix::net::UnixListener;
use std::time::{Duration, Instant};

const SOCKET_PATH: &str = "/run/aegisd.sock";

fn main() {
    let start = Instant::now();
    eprintln!("[aegisd] starting");

    // Remove stale socket if daemon was not cleanly shut down before.
    let _ = std::fs::remove_file(SOCKET_PATH);

    let listener = match UnixListener::bind(SOCKET_PATH) {
        Ok(l) => {
            eprintln!("[aegisd] listening on {}", SOCKET_PATH);
            l
        }
        Err(e) => {
            eprintln!("[aegisd] bind {}: {}", SOCKET_PATH, e);
            std::process::exit(1);
        }
    };

    // Restrict permissions so only root / aegis group can connect.
    let _ = std::fs::set_permissions(
        SOCKET_PATH,
        std::os::unix::fs::PermissionsExt::from_mode(0o660),
    );

    for stream in listener.incoming() {
        match stream {
            Ok(mut stream) => {
                if let Err(e) = handle_connection(&mut stream, start) {
                    eprintln!("[aegisd] connection error: {}", e);
                }
            }
            Err(e) => eprintln!("[aegisd] accept: {}", e),
        }
    }
}

fn handle_connection(
    stream: &mut std::os::unix::net::UnixStream,
    daemon_start: Instant,
) -> std::io::Result<()> {
    stream.set_read_timeout(Some(Duration::from_secs(5)))?;

    let mut reader = BufReader::new(stream.try_clone()?);

    // Read request line.
    let mut request_line = String::new();
    reader.read_line(&mut request_line)?;
    let request_line = request_line.trim().to_owned();

    // Drain headers.
    let mut content_length: usize = 0;
    loop {
        let mut header = String::new();
        reader.read_line(&mut header)?;
        let header = header.trim();
        if header.is_empty() { break; }
        if let Some(val) = header.to_ascii_lowercase().strip_prefix("content-length:") {
            content_length = val.trim().parse().unwrap_or(0);
        }
    }

    // Read body if present.
    let body = if content_length > 0 {
        let mut buf = vec![0u8; content_length.min(65_536)];
        use std::io::Read;
        reader.read_exact(&mut buf)?;
        String::from_utf8_lossy(&buf).into_owned()
    } else {
        String::new()
    };

    let (method, path) = parse_request_line(&request_line);
    let response = route(method, path, &body, daemon_start);

    stream.write_all(format!(
        "HTTP/1.0 {} OK\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
        response.status,
        response.body.len(),
        response.body
    ).as_bytes())?;
    stream.flush()?;
    Ok(())
}

fn parse_request_line(line: &str) -> (&str, &str) {
    let mut parts = line.splitn(3, ' ');
    let method = parts.next().unwrap_or("GET");
    let path   = parts.next().unwrap_or("/");
    (method, path)
}

struct Response {
    status: u16,
    body:   String,
}

impl Response {
    fn ok(body: String) -> Self { Self { status: 200, body } }
    fn not_found() -> Self {
        Self { status: 404, body: r#"{"error":"not found"}"#.to_owned() }
    }
    fn bad_request(msg: &str) -> Self {
        Self { status: 400, body: format!(r#"{{"error":"{}"}}"#, msg) }
    }
}

fn route(method: &str, path: &str, body: &str, start: Instant) -> Response {
    // Strip query string.
    let path = path.split('?').next().unwrap_or(path);

    match (method, path) {
        ("GET",  "/health")          => Response::ok(health::get_health(start)),
        ("GET",  "/services")        => Response::ok(services::list_services()),
        ("GET",  "/firewall/rules")  => Response::ok(firewall::list_rules()),
        ("GET",  "/logs")            => Response::ok(logs::get_logs()),
        ("GET",  "/vpn/status")      => Response::ok(vpn::get_status()),
        ("GET",  "/updates")         => Response::ok(updates::list_updates()),
        ("GET",  "/containers")      => Response::ok(containers::list_containers()),
        ("GET",  "/backups")         => Response::ok(backups::list_backups()),
        ("POST", "/firewall/rules")  => {
            match firewall::add_rule(body) {
                Ok(json) => Response::ok(json),
                Err(e)   => Response::bad_request(e),
            }
        }
        ("POST", p) if p.starts_with("/services/") && p.ends_with("/start") => {
            let name = extract_service_name(p, "/start");
            Response::ok(services::start_service(name))
        }
        ("POST", p) if p.starts_with("/services/") && p.ends_with("/stop") => {
            let name = extract_service_name(p, "/stop");
            Response::ok(services::stop_service(name))
        }
        ("DELETE", p) if p.starts_with("/firewall/rules/") => {
            let id_str = p.trim_start_matches("/firewall/rules/");
            match id_str.parse::<u32>() {
                Ok(id) => Response::ok(firewall::delete_rule(id)),
                Err(_) => Response::bad_request("invalid rule id"),
            }
        }
        _ => Response::not_found(),
    }
}

fn extract_service_name<'a>(path: &'a str, suffix: &str) -> &'a str {
    path.trim_start_matches("/services/")
        .trim_end_matches(suffix)
}
