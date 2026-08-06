//! Router route for RustMyAdmin.

use crate::aegisd_client::AegisdClient;

pub fn index() -> String {
    let router = AegisdClient::query("/router/status").unwrap_or_else(|e| format!(r#"{{"error":"{}"}}"#, e));
    let interfaces = AegisdClient::query("/network/interfaces").unwrap_or_else(|e| format!(r#"{{"error":"{}"}}"#, e));
    let radio = AegisdClient::query("/radio/status").unwrap_or_else(|e| format!(r#"{{"error":"{}"}}"#, e));
    let dns = AegisdClient::query("/dns/status").unwrap_or_else(|e| format!(r#"{{"error":"{}"}}"#, e));
    format!(
        r#"<!DOCTYPE html><html><head><meta charset="utf-8">
<title>AegisOS Admin - Router</title>
<style>body{{font-family:monospace;background:#1a1a2e;color:#eee;padding:2em}}
h1,h2{{color:#00d4ff}} pre{{background:#0f3460;padding:1em;border-radius:4px;overflow:auto}}
a{{color:#00d4ff}} nav a{{margin-right:1em}} .grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:1em}}</style>
</head><body>
<nav><a href="/">Dashboard</a><a href="/router">Router</a><a href="/services">Services</a>
<a href="/firewall">Firewall</a><a href="/vpn">VPN</a><a href="/containers">Containers</a>
<a href="/backups">Backups</a><a href="/updates">Updates</a><a href="/logs">Logs</a><a href="/system">System</a></nav>
<h1>ÆÐELRIC Router</h1>
<p>AegisBox router bring-up: LTE/SIM, WAN/LAN roles, DNS/DHCP, nftables, WireGuard.</p>
<div class="grid">
<section><h2>Router Status</h2><pre>{router}</pre></section>
<section><h2>Interfaces</h2><pre>{interfaces}</pre></section>
<section><h2>Radio</h2><pre>{radio}</pre></section>
<section><h2>DNS/DHCP</h2><pre>{dns}</pre></section>
</div>
</body></html>"#,
        router = html_escape(&router),
        interfaces = html_escape(&interfaces),
        radio = html_escape(&radio),
        dns = html_escape(&dns),
    )
}

pub fn api_status() -> String {
    AegisdClient::query("/router/status").unwrap_or_else(|e| format!(r#"{{"error":"{}"}}"#, e))
}

fn html_escape(s: &str) -> String {
    s.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;").replace('"', "&quot;")
}
