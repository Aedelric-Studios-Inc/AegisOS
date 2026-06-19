//! Service sub-commands for aegisctl.

use crate::{daemon_get, daemon_post};

pub fn run(args: &[String]) {
    let sub = args.first().map(String::as_str).unwrap_or("list");
    match sub {
        "list" => {
            match daemon_get("/services") {
                Ok(json) => println!("{}", json),
                Err(e)   => { eprintln!("aegisctl service list: {}", e); std::process::exit(1); }
            }
        }
        "start" => {
            let name = match args.get(1) {
                Some(n) => n.as_str(),
                None => { eprintln!("usage: aegisctl service start <name>"); std::process::exit(1); }
            };
            match daemon_post(&format!("/services/{}/start", name), "") {
                Ok(json) => println!("{}", json),
                Err(e)   => { eprintln!("aegisctl service start: {}", e); std::process::exit(1); }
            }
        }
        "stop" => {
            let name = match args.get(1) {
                Some(n) => n.as_str(),
                None => { eprintln!("usage: aegisctl service stop <name>"); std::process::exit(1); }
            };
            match daemon_post(&format!("/services/{}/stop", name), "") {
                Ok(json) => println!("{}", json),
                Err(e)   => { eprintln!("aegisctl service stop: {}", e); std::process::exit(1); }
            }
        }
        _ => {
            eprintln!("aegisctl service: unknown sub-command '{}'", sub);
            eprintln!("  sub-commands: list, start <name>, stop <name>");
            std::process::exit(1);
        }
    }
}
