//! AegisOS DNS filter — intercepts DNS queries and blocks flagged domains.

use std::collections::HashSet;
use std::env;
use std::net::UdpSocket;
use std::time::Duration;

const DEFAULT_LISTEN_ADDR: &str = "0.0.0.0:53";
const DEFAULT_UPSTREAM: &str = "1.1.1.1:53";
const BLOCKLIST: &[&str] = &[
    "ads.example.com",
    "tracking.example.com",
    "malware.example.com",
];

fn main() {
    let listen_addr = env::var("AEGIS_DNS_FILTER_LISTEN")
        .unwrap_or_else(|_| DEFAULT_LISTEN_ADDR.to_owned());
    let upstream_addr = env::var("AEGIS_DNS_FILTER_UPSTREAM")
        .unwrap_or_else(|_| DEFAULT_UPSTREAM.to_owned());
    println!("[dns-filter] service started, listening on {listen_addr}");
    let blocklist: HashSet<String> = BLOCKLIST.iter().map(|value| value.to_string()).collect();

    let socket = match UdpSocket::bind(&listen_addr) {
        Ok(socket) => socket,
        Err(error) => {
            eprintln!("[dns-filter] bind {listen_addr}: {error}");
            std::process::exit(1);
        }
    };
    socket.set_read_timeout(Some(Duration::from_secs(5))).ok();

    let upstream = match UdpSocket::bind("0.0.0.0:0") {
        Ok(socket) => socket,
        Err(error) => {
            eprintln!("[dns-filter] failed to create upstream socket: {error}");
            std::process::exit(1);
        }
    };
    upstream.set_read_timeout(Some(Duration::from_secs(2))).ok();

    let mut buffer = [0_u8; 512];
    loop {
        let (length, client) = match socket.recv_from(&mut buffer) {
            Ok(result) => result,
            Err(error) if error.kind() == std::io::ErrorKind::WouldBlock => continue,
            Err(error) if error.kind() == std::io::ErrorKind::TimedOut => continue,
            Err(error) => {
                eprintln!("[dns-filter] receive: {error}");
                continue;
            }
        };
        let query = &buffer[..length];

        let name = extract_qname(query);
        if blocklist.contains(&name) {
            eprintln!("[dns-filter] blocked: {name}");
            let response = make_nxdomain(query);
            socket.send_to(&response, client).ok();
            continue;
        }

        if upstream.send_to(query, &upstream_addr).is_ok() {
            let mut response_buffer = [0_u8; 512];
            if let Ok((response_length, _)) = upstream.recv_from(&mut response_buffer) {
                socket
                    .send_to(&response_buffer[..response_length], client)
                    .ok();
            }
        }
    }
}

fn extract_qname(packet: &[u8]) -> String {
    if packet.len() < 13 {
        return String::new();
    }
    let mut name = String::new();
    let mut index = 12_usize;
    loop {
        if index >= packet.len() {
            break;
        }
        let length = packet[index] as usize;
        if length == 0 {
            break;
        }
        index += 1;
        if index + length > packet.len() {
            break;
        }
        if !name.is_empty() {
            name.push('.');
        }
        name.push_str(&String::from_utf8_lossy(&packet[index..index + length]));
        index += length;
    }
    name
}

fn make_nxdomain(query: &[u8]) -> Vec<u8> {
    if query.len() < 2 {
        return Vec::new();
    }
    let mut response = query.to_vec();
    if response.len() >= 4 {
        response[2] = 0x81;
        response[3] = 0x83;
    }
    response
}
