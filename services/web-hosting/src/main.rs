//! AegisOS lightweight web hosting service.

use std::env;
use std::fs;
use std::io::{BufRead, BufReader, Write};
use std::net::{TcpListener, TcpStream};
use std::path::{Path, PathBuf};
use std::thread;

const DEFAULT_LISTEN_ADDR: &str = "0.0.0.0:80";
const DEFAULT_WEBROOT: &str = "/var/www/html";

fn main() {
    let listen_address = env::var("AEGIS_WEB_BIND")
        .unwrap_or_else(|_| DEFAULT_LISTEN_ADDR.to_owned());
    let webroot = env::var("AEGIS_WEBROOT")
        .unwrap_or_else(|_| DEFAULT_WEBROOT.to_owned());
    println!("[web-hosting] service started, listening on {listen_address}");
    let listener = match TcpListener::bind(&listen_address) {
        Ok(listener) => listener,
        Err(error) => {
            eprintln!("[web-hosting] bind {listen_address}: {error}");
            std::process::exit(1);
        }
    };
    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                let webroot = webroot.clone();
                thread::spawn(move || handle_connection(stream, &webroot));
            }
            Err(error) => eprintln!("[web-hosting] accept: {error}"),
        }
    }
}

fn handle_connection(mut stream: TcpStream, webroot: &str) {
    let reader = BufReader::new(&stream);
    let request_line = match reader.lines().next() {
        Some(Ok(line)) => line,
        _ => return,
    };
    let parts: Vec<&str> = request_line.splitn(3, ' ').collect();
    if parts.len() < 2 {
        return;
    }
    let raw_path = parts[1];

    let safe_path = raw_path.split("..").collect::<Vec<_>>().join("");
    let file_path: PathBuf = Path::new(webroot).join(safe_path.trim_start_matches('/'));
    let file_path = if file_path.is_dir() {
        file_path.join("index.html")
    } else {
        file_path
    };

    match fs::read(&file_path) {
        Ok(body) => {
            let content_type = mime_type(
                file_path
                    .extension()
                    .and_then(|extension| extension.to_str())
                    .unwrap_or(""),
            );
            let header = format!(
                "HTTP/1.1 200 OK\r\nContent-Type: {content_type}\r\nContent-Length: {}\r\n\r\n",
                body.len()
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

fn mime_type(extension: &str) -> &'static str {
    match extension {
        "html" | "htm" => "text/html",
        "css" => "text/css",
        "js" => "application/javascript",
        "json" => "application/json",
        "png" => "image/png",
        "jpg" | "jpeg" => "image/jpeg",
        "svg" => "image/svg+xml",
        _ => "application/octet-stream",
    }
}
