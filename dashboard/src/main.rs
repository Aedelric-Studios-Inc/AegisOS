//! AegisOS Dashboard — web management interface.

mod api;
mod auth;
mod cpu_accel;
mod routes;
mod state;

use std::io::{Read, Write};
use std::net::{IpAddr, TcpListener, TcpStream};

use routes::{dashboard, devices, firewall, logs, rustmyadmin, updates, vpn};
use state::AppState;

fn main() -> std::io::Result<()> {
    let state = AppState::new();
    let listener = TcpListener::bind("0.0.0.0:8080")?;
    println!("[dashboard] AegisOS Dashboard listening on http://0.0.0.0:8080");

    for incoming in listener.incoming() {
        match incoming {
            Ok(mut stream) => {
                if let Err(error) = handle_connection(&mut stream, &state) {
                    eprintln!("[dashboard] request error: {error}");
                }
            }
            Err(error) => eprintln!("[dashboard] accept error: {error}"),
        }
    }

    Ok(())
}

fn handle_connection(stream: &mut TcpStream, state: &AppState) -> std::io::Result<()> {
    let mut buffer = [0_u8; 4096];
    let read = stream.read(&mut buffer)?;
    if read == 0 {
        return Ok(());
    }

    let request = String::from_utf8_lossy(&buffer[..read]);
    let request_line = request.lines().next().unwrap_or_default();
    let mut parts = request_line.split_whitespace();
    let method = parts.next().unwrap_or_default();
    let path = parts.next().unwrap_or("/");
    let authorization = request
        .lines()
        .find_map(|line| line.strip_prefix("Authorization:"))
        .map(str::trim);
    let peer_ip = stream
        .peer_addr()
        .map(|address| address.ip())
        .unwrap_or(IpAddr::from([127, 0, 0, 1]));

    let response = if method != "GET" {
        Response::text(405, "Method Not Allowed", "Only GET is supported.")
    } else if path.starts_with("/api/") && !auth::authorize_api_request(authorization, peer_ip) {
        Response::text(
            401,
            "Unauthorized",
            "Set AEGIS_DASHBOARD_TOKEN and send it as an Authorization header for API access.",
        )
    } else {
        route_request(path, state)
    };

    stream.write_all(response.as_http().as_bytes())?;
    stream.flush()?;
    Ok(())
}

fn route_request(path: &str, state: &AppState) -> Response {
    let path = path.split('?').next().unwrap_or(path);

    match path {
        "/" | "/dashboard" => Response::html(200, "OK", dashboard::index(state)),
        "/devices" => Response::html(200, "OK", devices::index(state)),
        "/firewall" => Response::html(200, "OK", firewall::index(state)),
        "/vpn" => Response::html(200, "OK", vpn::index(state)),
        "/logs" => Response::html(200, "OK", logs::index(state)),
        "/rustmyadmin" => Response::html(200, "OK", rustmyadmin::index(state)),
        "/updates" => Response::html(200, "OK", updates::index(state)),
        "/api/status" => Response::json(200, "OK", api::get_status(state)),
        "/api/devices" => Response::json(200, "OK", api::get_devices(state)),
        "/api/rustmyadmin" => Response::json(200, "OK", api::get_rustmyadmin_catalog(state)),
        _ if path.starts_with("/rustmyadmin/") => match parse_admin_table_path(path) {
            Some((database_slug, table_slug)) => rustmyadmin::table_detail(state, database_slug, table_slug)
                .map(|body| Response::html(200, "OK", body))
                .unwrap_or_else(|| {
                    Response::text(404, "Not Found", "Requested RustMyAdmin table was not found.")
                }),
            None => Response::text(404, "Not Found", "Requested RustMyAdmin route was not found."),
        },
        _ if path.starts_with("/api/rustmyadmin/") => match parse_admin_table_path(
            path.trim_start_matches("/api"),
        ) {
            Some((database_slug, table_slug)) => api::get_rustmyadmin_table(state, database_slug, table_slug)
                .map(|body| Response::json(200, "OK", body))
                .unwrap_or_else(|| {
                    Response::text(404, "Not Found", "Requested RustMyAdmin table was not found.")
                }),
            None => Response::text(404, "Not Found", "Requested RustMyAdmin API route was not found."),
        },
        _ => Response::text(404, "Not Found", "AegisOS dashboard route not found."),
    }
}

fn parse_admin_table_path(path: &str) -> Option<(&str, &str)> {
    let path = path.strip_prefix("/rustmyadmin/")?;
    let (database_slug, table_slug) = path.split_once("/table/")?;
    if database_slug.is_empty() || table_slug.is_empty() {
        return None;
    }
    Some((database_slug, table_slug))
}

struct Response {
    status_code: u16,
    status_text: &'static str,
    content_type: &'static str,
    body: String,
}

impl Response {
    fn html(status_code: u16, status_text: &'static str, body: String) -> Self {
        Self {
            status_code,
            status_text,
            content_type: "text/html; charset=utf-8",
            body,
        }
    }

    fn json(status_code: u16, status_text: &'static str, body: String) -> Self {
        Self {
            status_code,
            status_text,
            content_type: "application/json; charset=utf-8",
            body,
        }
    }

    fn text(status_code: u16, status_text: &'static str, body: &str) -> Self {
        Self {
            status_code,
            status_text,
            content_type: "text/plain; charset=utf-8",
            body: body.to_owned(),
        }
    }

    fn as_http(&self) -> String {
        format!(
            "HTTP/1.1 {} {}\r\nContent-Type: {}\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
            self.status_code,
            self.status_text,
            self.content_type,
            self.body.len(),
            self.body
        )
    }
}
