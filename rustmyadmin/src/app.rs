//! HTTP request handling for RustMyAdmin.

use std::collections::HashMap;
use std::io::{Read, Write};
use std::net::{IpAddr, TcpStream};

use crate::auth;
use crate::routes;
use crate::security;

const MAX_REQUEST_BYTES: usize = 64 * 1024;

pub fn handle_connection(stream: &mut TcpStream, peer_ip: IpAddr) -> std::io::Result<()> {
    let request = read_http_request(stream)?;
    if request.method.is_empty() {
        return Ok(());
    }

    let path = request.path.split('?').next().unwrap_or(&request.path);
    let response = if !security::rate_limit::allow(peer_ip) {
        Response::text(429, "Too Many Requests")
    } else if matches!((request.method.as_str(), path), ("GET", "/login" | "/auth/login")) {
        Response::html(
            200,
            routes::auth::login_page(None, auth::authentication_configured()),
        )
    } else if matches!((request.method.as_str(), path), ("POST", "/auth/login")) {
        handle_login(&request, peer_ip)
    } else {
        handle_authenticated_request(&request, path, peer_ip)
    };

    stream.write_all(response.as_http().as_bytes())?;
    stream.flush()
}

fn handle_authenticated_request(request: &HttpRequest, path: &str, peer_ip: IpAddr) -> Response {
    let authorization = request.header("authorization");
    let browser_session = request
        .header("cookie")
        .and_then(|header| parse_cookie(header, auth::SESSION_COOKIE));

    let Some(principal) = auth::authorize_request(authorization, browser_session.as_deref(), peer_ip)
    else {
        return if path.starts_with("/api/") {
            Response::json(401, r#"{"error":"authentication required"}"#.to_owned())
        } else {
            Response::redirect("/login")
        };
    };

    if matches!((request.method.as_str(), path), ("GET" | "POST", "/logout" | "/auth/logout")) {
        if let Some(session_id) = principal.session_id.as_deref() {
            auth::sessions::revoke(session_id);
            auth::csrf::revoke(session_id);
        }
        let response = Response::redirect("/login")
            .with_header("Set-Cookie", clear_session_cookie_header());
        security::audit::record_access(
            peer_ip,
            &principal.username,
            &request.method,
            path,
            response.status,
        );
        return response;
    }

    if !security::permissions::can_access_route(principal.role, &request.method, path) {
        let response = Response::text(403, "Forbidden");
        security::audit::record_access(
            peer_ip,
            &principal.username,
            &request.method,
            path,
            response.status,
        );
        return response;
    }

    if requires_csrf(&request.method) && !principal.via_bearer {
        if let Some(session_id) = principal.session_id.as_deref() {
            let candidate = request
                .header("x-csrf-token")
                .map(str::to_owned)
                .or_else(|| routes::form_value(&request.body, "csrf"))
                .unwrap_or_default();
            if !auth::csrf::validate(session_id, &candidate) {
                let response = Response::text(403, "Invalid or expired CSRF token");
                security::audit::record_access(
                    peer_ip,
                    &principal.username,
                    &request.method,
                    path,
                    response.status,
                );
                return response;
            }
        }
    }

    let csrf_token = match principal.session_id.as_deref() {
        Some(session_id) => match auth::csrf::get_or_generate(session_id) {
            Ok(token) => token,
            Err(error) => {
                let response = Response::text(500, &format!("Could not create CSRF token: {error}"));
                security::audit::record_access(
                    peer_ip,
                    &principal.username,
                    &request.method,
                    path,
                    response.status,
                );
                return response;
            }
        },
        None => String::new(),
    };
    let context = routes::RequestContext {
        username: &principal.username,
        role: principal.role,
        csrf_token: &csrf_token,
    };
    let (status, content_type, body) =
        routes::dispatch(&request.method, path, &request.body, &context);
    let response = Response::new(status, content_type, body);
    security::audit::record_access(
        peer_ip,
        &principal.username,
        &request.method,
        path,
        response.status,
    );
    response
}

fn handle_login(request: &HttpRequest, peer_ip: IpAddr) -> Response {
    if !auth::authentication_configured() {
        return Response::html(
            503,
            routes::auth::login_page(
                Some("RustMyAdmin has no configured admin password or token."),
                false,
            ),
        );
    }

    let username = routes::form_value(&request.body, "username")
        .unwrap_or_else(|| "admin".to_owned());
    let secret = routes::form_value(&request.body, "secret").unwrap_or_default();

    match auth::authenticate_login(&username, &secret) {
        Some(principal) => match auth::sessions::create(&principal.username) {
            Ok(session_id) => {
                let response = Response::redirect("/dashboard")
                    .with_header("Set-Cookie", session_cookie_header(&session_id));
                security::audit::record_access(
                    peer_ip,
                    &principal.username,
                    &request.method,
                    "/auth/login",
                    response.status,
                );
                response
            }
            Err(error) => {
                let response = Response::html(
                    500,
                    routes::auth::login_page(
                        Some(&format!("Could not create browser session: {error}")),
                        true,
                    ),
                );
                security::audit::record_access(
                    peer_ip,
                    &principal.username,
                    &request.method,
                    "/auth/login",
                    response.status,
                );
                response
            }
        },
        None => {
            let response = Response::html(
                401,
                routes::auth::login_page(Some("Invalid username or secret."), true),
            );
            security::audit::record_access(
                peer_ip,
                "unauthenticated",
                &request.method,
                "/auth/login",
                response.status,
            );
            response
        }
    }
}

fn requires_csrf(method: &str) -> bool {
    matches!(method, "POST" | "PUT" | "PATCH" | "DELETE")
}

struct HttpRequest {
    method: String,
    path: String,
    headers: HashMap<String, String>,
    body: String,
}

impl HttpRequest {
    fn header(&self, name: &str) -> Option<&str> {
        self.headers
            .get(&name.to_ascii_lowercase())
            .map(String::as_str)
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
        if bytes.len() > MAX_REQUEST_BYTES {
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "RustMyAdmin request exceeded 64 KiB",
            ));
        }
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
    let content_length = parse_content_length(&header_text);
    let body_start = end + 4;
    let body_end = body_start.saturating_add(content_length).min(bytes.len());
    let body = String::from_utf8_lossy(&bytes[body_start..body_end]).into_owned();

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
            name.trim()
                .eq_ignore_ascii_case("Content-Length")
                .then(|| value.trim().parse::<usize>().ok())
                .flatten()
        })
        .unwrap_or(0)
}

fn parse_cookie(cookie_header: &str, name: &str) -> Option<String> {
    cookie_header.split(';').find_map(|cookie| {
        let (cookie_name, cookie_value) = cookie.trim().split_once('=')?;
        (cookie_name == name).then(|| cookie_value.to_owned())
    })
}

fn session_cookie_header(session_id: &str) -> String {
    format!(
        "{}={}; Path=/; HttpOnly; SameSite=Strict; Max-Age=43200",
        auth::SESSION_COOKIE,
        session_id
    )
}

fn clear_session_cookie_header() -> String {
    format!(
        "{}=deleted; Path=/; HttpOnly; SameSite=Strict; Max-Age=0",
        auth::SESSION_COOKIE
    )
}

struct Response {
    status: u16,
    content_type: &'static str,
    body: String,
    headers: Vec<(String, String)>,
}

impl Response {
    fn new(status: u16, content_type: &'static str, body: String) -> Self {
        Self {
            status,
            content_type,
            body,
            headers: Vec::new(),
        }
    }

    fn html(status: u16, body: String) -> Self {
        Self::new(status, "text/html; charset=utf-8", body)
    }

    fn json(status: u16, body: String) -> Self {
        Self::new(status, "application/json; charset=utf-8", body)
    }

    fn text(status: u16, body: &str) -> Self {
        Self::new(status, "text/plain; charset=utf-8", body.to_owned())
    }

    fn redirect(location: &str) -> Self {
        Self::text(303, "Redirecting...").with_header("Location", location.to_owned())
    }

    fn with_header(mut self, name: &str, value: String) -> Self {
        self.headers.push((name.to_owned(), value));
        self
    }

    fn as_http(&self) -> String {
        let mut response = format!(
            "HTTP/1.1 {} {}\r\nContent-Type: {}\r\nContent-Length: {}\r\n{}\r\nConnection: close\r\n",
            self.status,
            http_status_text(self.status),
            self.content_type,
            self.body.as_bytes().len(),
            security::headers::security_headers()
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

fn http_status_text(code: u16) -> &'static str {
    match code {
        200 => "OK",
        303 => "See Other",
        400 => "Bad Request",
        401 => "Unauthorized",
        403 => "Forbidden",
        404 => "Not Found",
        405 => "Method Not Allowed",
        413 => "Content Too Large",
        429 => "Too Many Requests",
        500 => "Internal Server Error",
        503 => "Service Unavailable",
        _ => "Unknown",
    }
}
