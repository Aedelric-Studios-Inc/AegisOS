//! Main RustMyAdmin dashboard.

use crate::aegisd_client::models::HealthResponse;
use crate::aegisd_client::AegisdClient;

use super::html_escape;

pub fn index() -> String {
    let raw = AegisdClient::query("/health");
    let summary = match &raw {
        Ok(json) => match HealthResponse::parse(json) {
            Ok(health) => format!(
                "<dl><dt>Status</dt><dd>{}</dd><dt>Uptime</dt><dd>{} seconds</dd></dl>",
                html_escape(&health.status),
                health.uptime_seconds
            ),
            Err(error) => format!("<p>Health response parse error: {}</p>", html_escape(&error.to_string())),
        },
        Err(error) => format!("<p>aegisd connection error: {}</p>", html_escape(&error.to_string())),
    };
    let raw = raw.unwrap_or_else(|error| format!(r#"{{"error":"{}"}}"#, error));

    format!(
        r#"<!DOCTYPE html><html><head><meta charset="utf-8"><title>AegisOS Admin - Dashboard</title>
<style>body{{font-family:monospace;background:#1a1a2e;color:#eee;padding:2em}}h1,dt{{color:#00d4ff}}pre{{background:#0f3460;padding:1em;border-radius:4px;overflow:auto}}a{{color:#00d4ff}}nav a{{margin-right:1em}}dt{{font-weight:bold}}dd{{margin:0 0 1em}}</style></head><body>
{nav}<h1>System Dashboard</h1><p>Live health data from aegisd.</p>{summary}<h2>Raw response</h2><pre>{raw}</pre></body></html>"#,
        nav = super::navigation(),
        raw = html_escape(&raw)
    )
}

pub fn api_list() -> String {
    AegisdClient::query("/health")
        .unwrap_or_else(|error| format!(r#"{{"error":"{}"}}"#, error))
}
