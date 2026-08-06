//! Route dispatch for RustMyAdmin.

pub mod auth;
pub mod backups;
pub mod containers;
pub mod dashboard;
pub mod firewall;
pub mod logs;
pub mod router;
pub mod services;
pub mod system;
pub mod updates;
pub mod users;
pub mod vpn;

use crate::auth::roles::Role;

pub struct RequestContext<'a> {
    pub username: &'a str,
    pub role: Role,
    pub csrf_token: &'a str,
}

pub fn dispatch(
    method: &str,
    path: &str,
    body: &str,
    context: &RequestContext<'_>,
) -> (u16, &'static str, String) {
    let path = path.split('?').next().unwrap_or(path);

    match (method, path) {
        ("GET", "/" | "/dashboard") => (200, "text/html", dashboard::index()),
        ("GET", "/services") => (200, "text/html", services::index(context.csrf_token)),
        ("GET", "/router") => (200, "text/html", router::index()),
        ("GET", "/firewall") => (200, "text/html", firewall::index()),
        ("GET", "/vpn") => (200, "text/html", vpn::index()),
        ("GET", "/containers") => (200, "text/html", containers::index()),
        ("GET", "/backups") => (200, "text/html", backups::index()),
        ("GET", "/updates") => (200, "text/html", updates::index()),
        ("GET", "/logs") => (200, "text/html", logs::index()),
        ("GET", "/users") => (200, "text/html", users::index(context)),
        ("GET", "/system") => (200, "text/html", system::index()),
        ("GET", "/api/dashboard") => (200, "application/json", dashboard::api_list()),
        ("GET", "/api/health") => (200, "application/json", system::api_health()),
        ("GET", "/api/services") => (200, "application/json", services::api_list()),
        ("GET", "/api/router") => (200, "application/json", router::api_status()),
        ("GET", "/api/firewall") => (200, "application/json", firewall::api_list()),
        ("GET", "/api/vpn") => (200, "application/json", vpn::api_list()),
        ("GET", "/api/containers") => (200, "application/json", containers::api_list()),
        ("GET", "/api/backups") => (200, "application/json", backups::api_list()),
        ("GET", "/api/updates") => (200, "application/json", updates::api_list()),
        ("GET", "/api/logs") => (200, "application/json", logs::api_list()),
        ("GET", "/api/users") => (200, "application/json", users::api_list()),
        ("POST", "/users/create") => action_result(users::create(body), "/users"),
        ("POST", path) if path.starts_with("/users/") && path.ends_with("/delete") => {
            let username = path
                .trim_start_matches("/users/")
                .trim_end_matches("/delete");
            action_result(users::delete(username), "/users")
        }
        ("POST", path) if path.starts_with("/services/") => {
            match parse_service_action(path) {
                Some((name, action)) => {
                    action_result(services::change_state(name, action), "/services")
                }
                None => (400, "text/plain", "Invalid service action path".to_owned()),
            }
        }
        _ if method != "GET" && method != "POST" => {
            (405, "text/plain", "Method not allowed".to_owned())
        }
        _ => (404, "text/plain", "Not found".to_owned()),
    }
}

fn action_result(result: Result<String, String>, return_path: &str) -> (u16, &'static str, String) {
    match result {
        Ok(message) => (
            200,
            "text/html",
            format!(
                "<!DOCTYPE html><html><body><h1>Action completed</h1><pre>{}</pre><p><a href=\"{}\">Return</a></p></body></html>",
                html_escape(&message),
                html_escape(return_path)
            ),
        ),
        Err(error) => (
            400,
            "text/html",
            format!(
                "<!DOCTYPE html><html><body><h1>Action failed</h1><pre>{}</pre><p><a href=\"{}\">Return</a></p></body></html>",
                html_escape(&error),
                html_escape(return_path)
            ),
        ),
    }
}

fn parse_service_action(path: &str) -> Option<(&str, &str)> {
    let rest = path.strip_prefix("/services/")?;
    let (name, action) = rest.rsplit_once('/')?;
    if name.is_empty() || !matches!(action, "start" | "stop") {
        return None;
    }
    Some((name, action))
}

pub fn navigation() -> &'static str {
    r#"<nav><a href="/">Dashboard</a><a href="/router">Router</a><a href="/services">Services</a><a href="/firewall">Firewall</a><a href="/vpn">VPN</a><a href="/containers">Containers</a><a href="/backups">Backups</a><a href="/updates">Updates</a><a href="/logs">Logs</a><a href="/users">Users</a><a href="/system">System</a><a href="/auth/logout">Sign out</a></nav>"#
}

pub fn form_value(body: &str, name: &str) -> Option<String> {
    body.split('&').find_map(|pair| {
        let (key, value) = pair.split_once('=')?;
        (percent_decode(key) == name).then(|| percent_decode(value))
    })
}

fn percent_decode(input: &str) -> String {
    let mut output = Vec::with_capacity(input.len());
    let mut bytes = input.as_bytes().iter().copied();
    while let Some(byte) = bytes.next() {
        match byte {
            b'+' => output.push(b' '),
            b'%' => {
                let high = bytes.next().and_then(hex_value);
                let low = bytes.next().and_then(hex_value);
                if let (Some(high), Some(low)) = (high, low) {
                    output.push((high << 4) | low);
                } else {
                    output.push(b'%');
                }
            }
            other => output.push(other),
        }
    }
    String::from_utf8_lossy(&output).into_owned()
}

fn hex_value(byte: u8) -> Option<u8> {
    match byte {
        b'0'..=b'9' => Some(byte - b'0'),
        b'a'..=b'f' => Some(byte - b'a' + 10),
        b'A'..=b'F' => Some(byte - b'A' + 10),
        _ => None,
    }
}

pub fn html_escape(input: &str) -> String {
    input
        .replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('"', "&quot;")
        .replace('\'', "&#39;")
}

pub fn json_escape(input: &str) -> String {
    let mut output = String::with_capacity(input.len());
    for character in input.chars() {
        match character {
            '\\' => output.push_str("\\\\"),
            '"' => output.push_str("\\\""),
            '\n' => output.push_str("\\n"),
            '\r' => output.push_str("\\r"),
            '\t' => output.push_str("\\t"),
            character if character.is_control() => {
                output.push_str(&format!("\\u{:04x}", character as u32));
            }
            character => output.push(character),
        }
    }
    output
}
