//! AegisOS Dashboard — web management interface.

mod api;
mod auth;
mod cpu_accel;
mod routes;
mod state;

use std::collections::HashMap;
use std::io::{Read, Write};
use std::net::{IpAddr, TcpListener, TcpStream};

use routes::{dashboard, devices, firewall, logs, rustmyadmin, updates, vpn};
use state::AppState;

fn main() -> std::io::Result<()> {
    let state = AppState::new();
    let listener = TcpListener::bind("0.0.0.0:4090")?;
    println!("[dashboard] AegisOS Dashboard listening on http://0.0.0.0:4090");

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
    let request = read_http_request(stream)?;
    if request.method.is_empty() {
        return Ok(());
    }

    let peer_ip = stream
        .peer_addr()
        .map(|address| address.ip())
        .unwrap_or(IpAddr::from([127, 0, 0, 1]));
    let path = request.path.split('?').next().unwrap_or(&request.path);
    let authorization = request.header("authorization");
    let cookie_header = request.header("cookie");
    let browser_session = cookie_header.and_then(|value| parse_cookie(value, auth::SESSION_COOKIE));

    let response = match (request.method.as_str(), path) {
        ("GET", "/login") => Response::html(200, "OK", login_page(None)),
        ("POST", "/login") => handle_login(&request.body, peer_ip),
        ("GET", "/logout") | ("POST", "/logout") => {
            auth::revoke_browser_session(browser_session.as_deref());
            Response::redirect("/login").with_header("Set-Cookie", clear_session_cookie_header())
        }
        _ if request.method != "GET" => {
            Response::text(405, "Method Not Allowed", "Only GET and POST /login are supported.")
        }
        _ if !auth::authorize_request(authorization, browser_session.as_deref(), peer_ip) => {
            if path.starts_with("/api/") {
                Response::text(401, "Unauthorized", "AegisOS Dashboard login required.")
            } else {
                Response::redirect("/login")
            }
        }
        _ => route_request(path, state),
    };

    stream.write_all(response.as_http().as_bytes())?;
    stream.flush()?;
    Ok(())
}

struct HttpRequest {
    method: String,
    path: String,
    headers: HashMap<String, String>,
    body: String,
}

impl HttpRequest {
    fn header(&self, name: &str) -> Option<&str> {
        self.headers.get(&name.to_ascii_lowercase()).map(String::as_str)
    }
}

fn read_http_request(stream: &mut TcpStream) -> std::io::Result<HttpRequest> {
    let mut buffer = [0_u8; 8192];
    let mut bytes = Vec::new();
    let mut header_end = None;

    loop {
        let read = stream.read(&mut buffer)?;
        if read == 0 {
            break;
        }
        bytes.extend_from_slice(&buffer[..read]);
        if header_end.is_none() {
            header_end = find_header_end(&bytes);
        }
        if let Some(end) = header_end {
            let header_text = String::from_utf8_lossy(&bytes[..end]);
            let content_length = parse_content_length(&header_text);
            let body_bytes = bytes.len().saturating_sub(end + 4);
            if body_bytes >= content_length {
                break;
            }
        }
        if bytes.len() > 64 * 1024 {
            break;
        }
    }

    let Some(end) = find_header_end(&bytes) else {
        return Ok(HttpRequest {
            method: String::new(),
            path: String::new(),
            headers: HashMap::new(),
            body: String::new(),
        });
    };

    let header_text = String::from_utf8_lossy(&bytes[..end]);
    let mut lines = header_text.lines();
    let request_line = lines.next().unwrap_or_default();
    let mut request_parts = request_line.split_whitespace();
    let method = request_parts.next().unwrap_or_default().to_owned();
    let path = request_parts.next().unwrap_or("/").to_owned();

    let headers = lines
        .filter_map(|line| {
            let (name, value) = line.split_once(':')?;
            Some((name.trim().to_ascii_lowercase(), value.trim().to_owned()))
        })
        .collect::<HashMap<_, _>>();

    let body = String::from_utf8_lossy(&bytes[end + 4..]).into_owned();

    Ok(HttpRequest {
        method,
        path,
        headers,
        body,
    })
}

fn find_header_end(bytes: &[u8]) -> Option<usize> {
    bytes.windows(4).position(|window| window == b"\r\n\r\n")
}

fn parse_content_length(headers: &str) -> usize {
    headers
        .lines()
        .find_map(|line| {
            let (name, value) = line.split_once(':')?;
            if name.trim().eq_ignore_ascii_case("Content-Length") {
                value.trim().parse::<usize>().ok()
            } else {
                None
            }
        })
        .unwrap_or(0)
}

fn handle_login(body: &str, peer_ip: IpAddr) -> Response {
    if !auth::dashboard_token_configured() {
        return Response::html(
            503,
            "Service Unavailable",
            login_page(Some("AEGIS_DASHBOARD_TOKEN is not set on the dashboard process.")),
        );
    }

    let submitted = form_value(body, "token").unwrap_or_default();
    match auth::authenticate(&submitted) {
        Some(session) => {
            match auth::create_browser_session(&session.user, peer_ip) {
                Ok(session_id) => Response::redirect("/dashboard")
                    .with_header("Set-Cookie", session_cookie_header(&session_id)),
                Err(error) => Response::html(
                    500,
                    "Internal Server Error",
                    login_page(Some(&format!("Could not create a browser session: {error}"))),
                ),
            }
        }
        None => Response::html(401, "Unauthorized", login_page(Some("Invalid dashboard token."))),
    }
}

fn login_page(error: Option<&str>) -> String {
    let error_html = error
        .map(|message| format!(r#"<div class="error">{}</div>"#, escape_html(message)))
        .unwrap_or_default();

    format!(
        r#"<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>AegisOS Dashboard Login</title>
  <style>
    :root {{
      color-scheme: dark;
      --bg: #08111f;
      --panel: #0f1b2d;
      --border: #1f3557;
      --text: #e8edf7;
      --muted: #92a7c7;
      --accent: #56b4ff;
      --bad: #ff5d73;
    }}
    * {{ box-sizing: border-box; }}
    body {{
      min-height: 100vh;
      margin: 0;
      display: grid;
      place-items: center;
      padding: 24px;
      font-family: Inter, system-ui, sans-serif;
      background:
        radial-gradient(circle at top, rgba(86, 180, 255, 0.20), transparent 35%),
        linear-gradient(180deg, #07101d 0%, #091728 100%);
      color: var(--text);
    }}
    .card {{
      width: min(460px, 100%);
      border: 1px solid var(--border);
      border-radius: 24px;
      padding: 28px;
      background: rgba(15, 27, 45, 0.94);
      box-shadow: 0 24px 60px rgba(0, 0, 0, 0.35);
    }}
    .mark {{
      width: 48px;
      height: 48px;
      border-radius: 14px;
      display: grid;
      place-items: center;
      color: var(--accent);
      border: 1px solid var(--border);
      background: rgba(86, 180, 255, 0.12);
      font-weight: 800;
      letter-spacing: 0.08em;
      margin-bottom: 18px;
    }}
    h1 {{ margin: 0; font-size: 1.7rem; }}
    p {{ margin: 10px 0 0; color: var(--muted); line-height: 1.5; }}
    form {{ margin-top: 22px; display: grid; gap: 14px; }}
    label {{ color: var(--muted); font-size: 0.92rem; }}
    input {{
      width: 100%;
      margin-top: 8px;
      padding: 13px 14px;
      border-radius: 14px;
      border: 1px solid var(--border);
      background: rgba(8, 17, 31, 0.82);
      color: var(--text);
      font-size: 1rem;
      outline: none;
    }}
    input:focus {{ border-color: var(--accent); box-shadow: 0 0 0 3px rgba(86, 180, 255, 0.16); }}
    button {{
      cursor: pointer;
      border: 0;
      border-radius: 14px;
      padding: 13px 16px;
      color: #06101d;
      background: var(--accent);
      font-weight: 800;
      font-size: 1rem;
    }}
    .error {{
      margin-top: 16px;
      border: 1px solid rgba(255, 93, 115, 0.45);
      background: rgba(255, 93, 115, 0.12);
      color: #ffd6dc;
      border-radius: 14px;
      padding: 12px 14px;
    }}
    .hint {{ font-size: 0.9rem; }}
    code {{ color: var(--text); }}
  </style>
</head>
<body>
  <main class="card">
    <div class="mark">Æ</div>
    <h1>AegisOS Dashboard</h1>
    <p>Sign in with the dashboard token configured in <code>AEGIS_DASHBOARD_TOKEN</code>.</p>
    {error_html}
    <form method="post" action="/login">
      <label>
        Dashboard token
        <input name="token" type="password" autocomplete="current-password" autofocus required>
      </label>
      <button type="submit">Unlock dashboard</button>
    </form>
    <p class="hint">This creates a local browser session cookie, so mobile browsers do not need a custom Authorization header.</p>
  </main>
</body>
</html>"#
    )
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

fn parse_cookie(cookie_header: &str, name: &str) -> Option<String> {
    cookie_header.split(';').find_map(|cookie| {
        let (cookie_name, cookie_value) = cookie.trim().split_once('=')?;
        if cookie_name == name {
            Some(cookie_value.to_owned())
        } else {
            None
        }
    })
}

fn form_value(body: &str, name: &str) -> Option<String> {
    body.split('&').find_map(|pair| {
        let (key, value) = pair.split_once('=')?;
        if percent_decode(key) == name {
            Some(percent_decode(value))
        } else {
            None
        }
    })
}

fn percent_decode(input: &str) -> String {
    let mut output = Vec::with_capacity(input.len());
    let mut bytes = input.as_bytes().iter().copied();

    while let Some(byte) = bytes.next() {
        match byte {
            b'+' => output.push(b' '),
            b'%' => {
                let high = bytes.next();
                let low = bytes.next();
                match (high.and_then(hex_value), low.and_then(hex_value)) {
                    (Some(high), Some(low)) => output.push((high << 4) | low),
                    _ => output.push(b'%'),
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

fn session_cookie_header(session_id: &str) -> String {
    format!(
        "{}={}; Path=/; HttpOnly; SameSite=Lax; Max-Age=43200",
        auth::SESSION_COOKIE,
        session_id
    )
}

fn clear_session_cookie_header() -> String {
    format!(
        "{}=deleted; Path=/; HttpOnly; SameSite=Lax; Max-Age=0",
        auth::SESSION_COOKIE
    )
}

fn escape_html(input: &str) -> String {
    input
        .replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('"', "&quot;")
        .replace('\'', "&#39;")
}

struct Response {
    status_code: u16,
    status_text: &'static str,
    content_type: &'static str,
    body: String,
    headers: Vec<(String, String)>,
}

impl Response {
    fn html(status_code: u16, status_text: &'static str, body: String) -> Self {
        Self {
            status_code,
            status_text,
            content_type: "text/html; charset=utf-8",
            body,
            headers: Vec::new(),
        }
    }

    fn json(status_code: u16, status_text: &'static str, body: String) -> Self {
        Self {
            status_code,
            status_text,
            content_type: "application/json; charset=utf-8",
            body,
            headers: Vec::new(),
        }
    }

    fn text(status_code: u16, status_text: &'static str, body: &str) -> Self {
        Self {
            status_code,
            status_text,
            content_type: "text/plain; charset=utf-8",
            body: body.to_owned(),
            headers: Vec::new(),
        }
    }

    fn redirect(location: &str) -> Self {
        Self {
            status_code: 303,
            status_text: "See Other",
            content_type: "text/plain; charset=utf-8",
            body: "Redirecting...".to_owned(),
            headers: vec![("Location".to_owned(), location.to_owned())],
        }
    }

    fn with_header(mut self, name: &str, value: String) -> Self {
        self.headers.push((name.to_owned(), value));
        self
    }

    fn as_http(&self) -> String {
        let mut response = format!(
            "HTTP/1.1 {} {}\r\nContent-Type: {}\r\nContent-Length: {}\r\nConnection: close\r\n",
            self.status_code,
            self.status_text,
            self.content_type,
            self.body.as_bytes().len()
        );

        for (name, value) in &self.headers {
            response.push_str(name);
            response.push_str(": ");
            response.push_str(value);
            response.push_str("\r\n");
        }

        response.push_str("\r\n");
        response.push_str(&self.body);
        response
    }
}
