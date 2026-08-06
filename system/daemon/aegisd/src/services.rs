//! Service orchestration for aegisd.
//! Proxies service status and control requests to the AegisOS service manager.

use std::env;
use std::io::{Read, Write};
use std::os::unix::net::UnixStream;
use std::time::Duration;

const DEFAULT_SERVICE_MANAGER_SOCKET: &str = "/run/service-manager.sock";

pub fn list_services() -> String {
    proxy_request("GET", "/services")
}

pub fn start_service(name: &str) -> String {
    control_service(name, "start")
}

pub fn stop_service(name: &str) -> String {
    control_service(name, "stop")
}

fn control_service(name: &str, action: &str) -> String {
    let name = canonical_service_name(name);
    if !valid_service_name(&name) {
        return error_json("invalid service name", Some(&name));
    }
    proxy_request("POST", &format!("/services/{}/{}", name, action))
}

fn canonical_service_name(name: &str) -> String {
    if name == "aegisd" {
        return name.to_owned();
    }
    name.strip_prefix("aegis-").unwrap_or(name).to_owned()
}

fn valid_service_name(name: &str) -> bool {
    !name.is_empty()
        && name
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || byte == b'-' || byte == b'_')
}

fn service_manager_socket() -> String {
    env::var("AEGIS_SERVICE_MANAGER_SOCKET")
        .unwrap_or_else(|_| DEFAULT_SERVICE_MANAGER_SOCKET.to_owned())
}

fn proxy_request(method: &str, path: &str) -> String {
    match send_request(method, path) {
        Ok(body) => body,
        Err(error) => error_json(&error, None),
    }
}

fn send_request(method: &str, path: &str) -> Result<String, String> {
    let socket_path = service_manager_socket();
    let mut stream = UnixStream::connect(&socket_path).map_err(|e| {
        format!(
            "service manager unavailable at {}: {}",
            socket_path, e
        )
    })?;

    stream
        .set_read_timeout(Some(Duration::from_secs(5)))
        .map_err(|e| e.to_string())?;
    stream
        .set_write_timeout(Some(Duration::from_secs(5)))
        .map_err(|e| e.to_string())?;

    let request = format!(
        "{} {} HTTP/1.0\r\nHost: localhost\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
        method, path
    );
    stream
        .write_all(request.as_bytes())
        .map_err(|e| e.to_string())?;

    let mut response = String::new();
    stream
        .read_to_string(&mut response)
        .map_err(|e| e.to_string())?;

    let status = response.lines().next().unwrap_or("");
    let body = response
        .split_once("\r\n\r\n")
        .map(|(_, body)| body.trim().to_owned())
        .unwrap_or_else(|| response.trim().to_owned());

    if status.contains(" 200 ") {
        Ok(body)
    } else if body.is_empty() {
        Err(format!("service manager returned {}", status))
    } else {
        Ok(body)
    }
}

fn error_json(message: &str, service: Option<&str>) -> String {
    match service {
        Some(service) => format!(
            r#"{{"error":"{}","service":"{}"}}"#,
            json_escape(message),
            json_escape(service)
        ),
        None => format!(r#"{{"error":"{}"}}"#, json_escape(message)),
    }
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
