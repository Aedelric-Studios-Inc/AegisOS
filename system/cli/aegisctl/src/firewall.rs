//! Firewall sub-commands for aegisctl.

use crate::{daemon_get, daemon_post};

pub fn run(args: &[String]) {
    let sub = args.first().map(String::as_str).unwrap_or("list");
    match sub {
        "list" => {
            match daemon_get("/firewall/rules") {
                Ok(json) => println!("{}", json),
                Err(e)   => { eprintln!("aegisctl firewall list: {}", e); std::process::exit(1); }
            }
        }
        "add" => {
            let body = build_rule_json(&args[1..]);
            match daemon_post("/firewall/rules", &body) {
                Ok(json) => println!("{}", json),
                Err(e)   => { eprintln!("aegisctl firewall add: {}", e); std::process::exit(1); }
            }
        }
        "del" => {
            let id = match args.get(1) {
                Some(n) => n.as_str(),
                None => { eprintln!("usage: aegisctl firewall del <id>"); std::process::exit(1); }
            };
            match daemon_post(&format!("/firewall/rules/{}", id), "") {
                Ok(json) => println!("{}", json),
                Err(e)   => { eprintln!("aegisctl firewall del: {}", e); std::process::exit(1); }
            }
        }
        _ => {
            eprintln!("aegisctl firewall: unknown sub-command '{}'", sub);
            eprintln!("  sub-commands: list, add [--chain C] [--proto P] [--src S] [--dst D] [--port N] [--action A], del <id>");
            std::process::exit(1);
        }
    }
}

fn build_rule_json(args: &[String]) -> String {
    let mut chain    = "INPUT";
    let mut protocol = "tcp";
    let mut src      = "any";
    let mut dst      = "any";
    let mut port     = "";
    let mut action   = "DROP";

    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "--chain"  => { if let Some(v) = args.get(i+1) { chain    = v; i+=1; } }
            "--proto"  => { if let Some(v) = args.get(i+1) { protocol = v; i+=1; } }
            "--src"    => { if let Some(v) = args.get(i+1) { src      = v; i+=1; } }
            "--dst"    => { if let Some(v) = args.get(i+1) { dst      = v; i+=1; } }
            "--port"   => { if let Some(v) = args.get(i+1) { port     = v; i+=1; } }
            "--action" => { if let Some(v) = args.get(i+1) { action   = v; i+=1; } }
            _ => {}
        }
        i += 1;
    }

    if port.is_empty() {
        format!(
            r#"{{"chain":"{}","protocol":"{}","src":"{}","dst":"{}","action":"{}"}}"#,
            chain, protocol, src, dst, action
        )
    } else {
        format!(
            r#"{{"chain":"{}","protocol":"{}","src":"{}","dst":"{}","port":{},"action":"{}"}}"#,
            chain, protocol, src, dst, port, action
        )
    }
}
