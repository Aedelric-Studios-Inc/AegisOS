//! Container management for aegisd.

use std::process::Command;

pub fn list_containers() -> String {
    // Query running containers via `podman ps --format json` if available,
    // otherwise fall back to an empty list.
    let output = Command::new("podman")
        .args(["ps", "--format", "{{.Names}}\t{{.Status}}\t{{.Image}}"])
        .output();

    match output {
        Ok(out) if out.status.success() => {
            let text = String::from_utf8_lossy(&out.stdout);
            let entries: Vec<String> = text
                .lines()
                .filter(|l| !l.trim().is_empty())
                .map(|l| {
                    let mut cols = l.splitn(3, '\t');
                    let name   = cols.next().unwrap_or("").replace('"', "\\\"");
                    let status = cols.next().unwrap_or("").replace('"', "\\\"");
                    let image  = cols.next().unwrap_or("").replace('"', "\\\"");
                    format!(r#"{{"name":"{}","status":"{}","image":"{}"}}"#, name, status, image)
                })
                .collect();
            format!(r#"{{"containers":[{}]}}"#, entries.join(","))
        }
        _ => r#"{"containers":[]}"#.to_owned(),
    }
}
