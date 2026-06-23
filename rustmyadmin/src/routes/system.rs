//! system route for rustmyadmin.

use crate::aegisd_client::AegisdClient;

pub fn index() -> String {
    let data = AegisdClient::query("/health").unwrap_or_else(|e| {
        format!(r#"{{"error":"{}"}}"#, e)
    });
    let hostname = std::env::var("HOSTNAME").unwrap_or_else(|_| "aegisbox".to_owned());
    format!(
        r#"<!DOCTYPE html><html><head><meta charset="utf-8">
<title>AegisOS Admin - System</title>
<style>body{{font-family:monospace;background:#1a1a2e;color:#eee;padding:2em}}
h1{{color:#00d4ff}} pre{{background:#0f3460;padding:1em;border-radius:4px;overflow:auto}}
a{{color:#00d4ff}} nav a{{margin-right:1em}}</style>
</head><body>
<nav><a href="/">Dashboard</a><a href="/router">Router</a><a href="/services">Services</a>
<a href="/firewall">Firewall</a><a href="/vpn">VPN</a>
<a href="/containers">Containers</a><a href="/backups">Backups</a>
<a href="/updates">Updates</a><a href="/logs">Logs</a>
<a href="/system">System</a></nav>
<h1>System Information</h1>
<p>Hostname: {hostname}</p>
<pre>{data}</pre>
</body></html>"#,
        hostname = html_escape(&hostname),
        data = html_escape(&data)
    )
}

pub fn api_health() -> String {
    AegisdClient::query("/health").unwrap_or_else(|e| {
        format!(r#"{{"error":"{}"}}"#, e)
    })
}

fn html_escape(s: &str) -> String {
    s.replace('&', "&amp;")
     .replace('<', "&lt;")
     .replace('>', "&gt;")
     .replace('"', "&quot;")
}
