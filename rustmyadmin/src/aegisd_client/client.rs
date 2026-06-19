//! aegisd client HTTP implementation.

use super::AegisdClient;

impl AegisdClient {
    /// Send a POST request to `path` with the given body.
    pub fn post(path: &str, body: &str) -> Result<String, String> {
        use std::io::{Read, Write};
        use std::os::unix::net::UnixStream;

        let mut stream = UnixStream::connect(super::SOCKET_PATH)
            .map_err(|e| format!("connect: {}", e))?;
        stream.set_read_timeout(Some(super::TIMEOUT)).ok();
        stream.set_write_timeout(Some(super::TIMEOUT)).ok();

        let request = format!(
            "POST {} HTTP/1.0\r\nHost: localhost\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
            path,
            body.len(),
            body
        );
        stream.write_all(request.as_bytes()).map_err(|e| e.to_string())?;
        let mut response = String::new();
        stream.read_to_string(&mut response).map_err(|e| e.to_string())?;
        if let Some(pos) = response.find("\r\n\r\n") {
            Ok(response[pos + 4..].to_owned())
        } else {
            Ok(response)
        }
    }
}
