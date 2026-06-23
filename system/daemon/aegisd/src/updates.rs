//! Update management for aegisd.

use std::process::Command;

pub fn list_updates() -> String {
    // Query available updates from the system package manager.
    // On AegisOS the update tool is `aegis-update list`.
    let output = Command::new("aegis-update").arg("list").output();

    match output {
        Ok(out) if out.status.success() => {
            let text = String::from_utf8_lossy(&out.stdout);
            let entries: Vec<String> = text
                .lines()
                .filter(|l| !l.trim().is_empty())
                .map(|l| format!("\"{}\"", l.replace('"', "\\\"")))
                .collect();
            format!(r#"{{"updates":[{}]}}"#, entries.join(","))
        }
        Ok(out) => {
            let stderr = String::from_utf8_lossy(&out.stderr);
            format!(
                r#"{{"updates":[],"error":"{}"}}"#,
                stderr
                    .lines()
                    .next()
                    .unwrap_or("no updates")
                    .replace('"', "\\\"")
            )
        }
        Err(_) => {
            // aegis-update not yet installed; report empty list.
            r#"{"updates":[]}"#.to_owned()
        }
    }
}
