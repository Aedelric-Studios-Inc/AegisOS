//! AegisOS lightweight web hosting service.

use std::io::{BufRead, BufReader, Write};
use std::net::{TcpListener, TcpStream};
use std::thread;
use std::fs;
use std::path::{Path, PathBuf};

const LISTEN_ADDR: &str = "0.0.0.0:80";
const WEBROOT:     &str = "/var/www/html";

fn main() {
    println!("[web-hosting] service started, listening on :80");
    let listener = match TcpListener::bind(LISTEN_ADDR) {
        Ok(l) => l,
        Err(e) => {
            eprintln!("[web-hosting] bind {}: {}", LISTEN_ADDR, e);
            loop { std::thread::sleep(std::time::Duration::from_secs(60)); }
        }
    };
    for stream in listener.incoming() {
        match stream {
            Ok(s) => { thread::spawn(|| handle_connection(s)); }
            Err(e) => eprintln!("[web-hosting] accept: {}", e),
        }
    }
}

fn handle_connection(mut stream: TcpStream) {
    let reader = BufReader::new(&stream);
    let request_line = match reader.lines().next() {
        Some(Ok(l)) => l,
        _ => return,
    };
    // Parse: GET /path HTTP/1.1
    let parts: Vec<&str> = request_line.splitn(3, ' ').collect();
    if parts.len() < 2 { return; }
    let raw_path = parts[1];

    // Prevent directory traversal
    let safe_path = raw_path.split("..").collect::<Vec<_>>().join("");
    let file_path: PathBuf = Path::new(WEBROOT).join(safe_path.trim_start_matches('/'));
    let file_path = if file_path.is_dir() { file_path.join("index.html") } else { file_path };

    match fs::read(&file_path) {
        Ok(body) => {
            let content_type = mime_type(file_path.extension()
                .and_then(|e| e.to_str()).unwrap_or(""));
            let header = format!(
                "HTTP/1.1 200 OK\r\nContent-Type: {}\r\nContent-Length: {}\r\n\r\n",
                content_type, body.len()
            );
            let _ = stream.write_all(header.as_bytes());
            let _ = stream.write_all(&body);
        }
        Err(_) => {
            let body = b"404 Not Found\n";
            let header = format!(
                "HTTP/1.1 404 Not Found\r\nContent-Length: {}\r\n\r\n",
                body.len()
            );
            let _ = stream.write_all(header.as_bytes());
            let _ = stream.write_all(body);
        }
    }
}

fn mime_type(ext: &str) -> &'static str {
    match ext {
        "html" | "htm" => "text/html",
        "css"          => "text/css",
        "js"           => "application/javascript",
        "json"         => "application/json",
        "png"          => "image/png",
        "jpg" | "jpeg" => "image/jpeg",
        "svg"          => "image/svg+xml",
        _              => "application/octet-stream",
    }
}
