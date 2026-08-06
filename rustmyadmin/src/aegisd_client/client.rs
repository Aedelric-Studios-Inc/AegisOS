//! aegisd Unix-socket HTTP client implementation.

use std::io::{Read, Write};
use std::os::unix::net::UnixStream;

use super::errors::AegisdError;
use super::{socket_path, AegisdClient, TIMEOUT};

impl AegisdClient {
    pub fn query(path: &str) -> Result<String, AegisdError> {
        Self::request("GET", path, "")
    }

    pub fn post(path: &str, body: &str) -> Result<String, AegisdError> {
        Self::request("POST", path, body)
    }

    fn request(method: &str, path: &str, body: &str) -> Result<String, AegisdError> {
        let socket_path = socket_path();
        let mut stream = UnixStream::connect(&socket_path)?;
        stream.set_read_timeout(Some(TIMEOUT))?;
        stream.set_write_timeout(Some(TIMEOUT))?;

        let authorization = std::env::var("AEGISD_TOKEN")
            .ok()
            .filter(|token| !token.trim().is_empty())
            .map(|token| format!("Authorization: Bearer {}\r\n", token.trim()))
            .unwrap_or_default();

        let request = format!(
            "{method} {path} HTTP/1.0\r\nHost: localhost\r\n{authorization}Content-Length: {}\r\nConnection: close\r\n\r\n{body}",
            body.as_bytes().len()
        );
        stream.write_all(request.as_bytes())?;

        let mut response = String::new();
        stream.read_to_string(&mut response)?;
        parse_response(&response)
    }
}

fn parse_response(response: &str) -> Result<String, AegisdError> {
    let (headers, body) = response.split_once("\r\n\r\n").ok_or_else(|| {
        AegisdError::Protocol("aegisd response did not contain an HTTP header terminator".to_owned())
    })?;

    let status_line = headers.lines().next().unwrap_or_default();
    let status = status_line
        .split_whitespace()
        .nth(1)
        .and_then(|value| value.parse::<u16>().ok())
        .ok_or_else(|| AegisdError::Protocol("aegisd response status was invalid".to_owned()))?;

    if (200..300).contains(&status) {
        Ok(body.to_owned())
    } else {
        Err(AegisdError::Protocol(format!(
            "aegisd returned HTTP {status}: {body}"
        )))
    }
}
