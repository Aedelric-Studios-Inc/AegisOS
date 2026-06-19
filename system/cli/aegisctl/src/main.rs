//! aegisctl — AegisOS system control CLI.
//!
//! Communicates with aegisd over its Unix socket to manage the system.
//!
//! Usage:
//!   aegisctl service list
//!   aegisctl service start <name>
//!   aegisctl service stop  <name>
//!   aegisctl firewall list
//!   aegisctl firewall add  --chain <chain> --proto <proto> --src <src> --dst <dst> --port <port> --action <action>
//!   aegisctl firewall del  <id>
//!   aegisctl vpn status
//!   aegisctl updates list
//!   aegisctl containers list
//!   aegisctl backups list
//!   aegisctl health

mod commands;
mod firewall;
mod service;

use std::env;
use std::io::{Read, Write};
use std::os::unix::net::UnixStream;
use std::time::Duration;

const SOCKET_PATH: &str = "/run/aegisd.sock";

fn main() {
    let args: Vec<String> = env::args().skip(1).collect();
    if args.is_empty() {
        print_usage();
        std::process::exit(1);
    }

    match args[0].as_str() {
        "service"    => service::run(&args[1..]),
        "firewall"   => firewall::run(&args[1..]),
        "vpn"        => cmd_vpn(&args[1..]),
        "updates"    => cmd_simple("/updates", &args[1..]),
        "containers" => cmd_simple("/containers", &args[1..]),
        "backups"    => cmd_simple("/backups", &args[1..]),
        "health"     => cmd_simple("/health", &[]),
        _            => { eprintln!("aegisctl: unknown command '{}'", args[0]); print_usage(); std::process::exit(1); }
    }
}

fn print_usage() {
    eprintln!("AegisOS system control CLI");
    eprintln!("Usage:");
    eprintln!("  aegisctl service    list|start <name>|stop <name>");
    eprintln!("  aegisctl firewall   list|add [options]|del <id>");
    eprintln!("  aegisctl vpn        status");
    eprintln!("  aegisctl updates    list");
    eprintln!("  aegisctl containers list");
    eprintln!("  aegisctl backups    list");
    eprintln!("  aegisctl health");
}

fn cmd_vpn(args: &[String]) {
    let sub = args.first().map(String::as_str).unwrap_or("status");
    if sub == "status" {
        cmd_simple("/vpn/status", &[]);
    } else {
        eprintln!("aegisctl vpn: unknown sub-command '{}'", sub);
        std::process::exit(1);
    }
}

fn cmd_simple(path: &str, args: &[String]) {
    let sub = args.first().map(String::as_str).unwrap_or("list");
    if sub != "list" && sub != "status" && sub != "check" {
        eprintln!("aegisctl: unrecognised argument '{}'", sub);
        std::process::exit(1);
    }
    match daemon_get(path) {
        Ok(json) => println!("{}", json),
        Err(e)   => { eprintln!("aegisctl: {}", e); std::process::exit(1); }
    }
}

/// Send a GET request to the aegisd socket and print the response body.
pub fn daemon_get(path: &str) -> Result<String, String> {
    let mut stream = UnixStream::connect(SOCKET_PATH)
        .map_err(|e| format!("cannot connect to aegisd ({}): {}", SOCKET_PATH, e))?;
    stream.set_read_timeout(Some(Duration::from_secs(5))).ok();
    stream.set_write_timeout(Some(Duration::from_secs(5))).ok();

    let req = format!("GET {} HTTP/1.0\r\nHost: localhost\r\nConnection: close\r\n\r\n", path);
    stream.write_all(req.as_bytes()).map_err(|e| e.to_string())?;

    let mut resp = String::new();
    stream.read_to_string(&mut resp).map_err(|e| e.to_string())?;

    Ok(if let Some(pos) = resp.find("\r\n\r\n") {
        resp[pos + 4..].trim().to_owned()
    } else {
        resp.trim().to_owned()
    })
}

/// Send a POST request to the aegisd socket.
pub fn daemon_post(path: &str, body: &str) -> Result<String, String> {
    let mut stream = UnixStream::connect(SOCKET_PATH)
        .map_err(|e| format!("cannot connect to aegisd ({}): {}", SOCKET_PATH, e))?;
    stream.set_read_timeout(Some(Duration::from_secs(5))).ok();
    stream.set_write_timeout(Some(Duration::from_secs(5))).ok();

    let req = format!(
        "POST {} HTTP/1.0\r\nHost: localhost\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
        path, body.len(), body
    );
    stream.write_all(req.as_bytes()).map_err(|e| e.to_string())?;

    let mut resp = String::new();
    stream.read_to_string(&mut resp).map_err(|e| e.to_string())?;

    Ok(if let Some(pos) = resp.find("\r\n\r\n") {
        resp[pos + 4..].trim().to_owned()
    } else {
        resp.trim().to_owned()
    })
}
