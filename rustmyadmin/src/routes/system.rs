//! System information and persisted RustMyAdmin configuration.

use crate::aegisd_client::AegisdClient;
use crate::db;

use super::html_escape;

pub fn index() -> String {
    let health = AegisdClient::query("/health")
        .unwrap_or_else(|error| format!(r#"{{"error":"{}"}}"#, error));
    let hostname = std::env::var("HOSTNAME").unwrap_or_else(|_| "aegisbox".to_owned());
    let config_rows = db::pool::list_config_entries()
        .into_iter()
        .map(|entry| {
            format!(
                "<tr><td>{}</td><td>{}</td></tr>",
                html_escape(&entry.key),
                html_escape(&entry.value)
            )
        })
        .collect::<String>();

    format!(
        r#"<!DOCTYPE html><html><head><meta charset="utf-8"><title>AegisOS Admin - System</title>
<style>body{{font-family:monospace;background:#1a1a2e;color:#eee;padding:2em}}h1,h2{{color:#00d4ff}}pre{{background:#0f3460;padding:1em;overflow:auto}}table{{border-collapse:collapse}}th,td{{border:1px solid #31547e;padding:.65em}}a{{color:#00d4ff}}nav a{{margin-right:1em}}</style></head><body>
{nav}<h1>System Information</h1><p>Hostname: {hostname}</p><pre>{health}</pre>
<h2>RustMyAdmin configuration store</h2><table><thead><tr><th>Key</th><th>Value</th></tr></thead><tbody>{config_rows}</tbody></table></body></html>"#,
        nav = super::navigation(),
        hostname = html_escape(&hostname),
        health = html_escape(&health)
    )
}

pub fn api_health() -> String {
    AegisdClient::query("/health")
        .unwrap_or_else(|error| format!(r#"{{"error":"{}"}}"#, error))
}
