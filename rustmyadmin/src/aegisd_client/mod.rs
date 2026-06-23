//! aegisd client — sends requests to the aegisd Unix socket.

pub mod errors;
pub mod models;

use std::io::{Read, Write};
use std::os::unix::net::UnixStream;
use std::time::Duration;

const SOCKET_PATH: &str = "/run/aegisd.sock";
const TIMEOUT: Duration  = Duration::from_secs(5);

pub struct AegisdClient;

impl AegisdClient {
    /// Send a GET request to `path` on the aegisd socket and return the JSON body.
    pub fn query(path: &str) -> Result<String, String> {
        let mut stream = UnixStream::connect(SOCKET_PATH)
            .map_err(|e| format!("connect {}: {}", SOCKET_PATH, e))?;
        stream.set_read_timeout(Some(TIMEOUT))
            .map_err(|e| e.to_string())?;
        stream.set_write_timeout(Some(TIMEOUT))
            .map_err(|e| e.to_string())?;

        let request = format!(
            "GET {} HTTP/1.0\r\nHost: localhost\r\nConnection: close\r\n\r\n",
            path
        );
        stream.write_all(request.as_bytes())
            .map_err(|e| e.to_string())?;

        let mut response = String::new();
        stream.read_to_string(&mut response)
            .map_err(|e| e.to_string())?;

        // Strip HTTP headers: body starts after the first blank line.
        if let Some(pos) = response.find("\r\n\r\n") {
            Ok(response[pos + 4..].to_owned())
        } else {
            Ok(response)
        }
    }
}
