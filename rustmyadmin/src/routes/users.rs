//! users route for rustmyadmin.

pub fn index() -> String {
    r#"<!DOCTYPE html><html><head><meta charset="utf-8">
<title>AegisOS Admin - Users</title>
<style>body{font-family:monospace;background:#1a1a2e;color:#eee;padding:2em}
h1{color:#00d4ff} a{color:#00d4ff} nav a{margin-right:1em}</style>
</head><body>
<nav><a href="/">Dashboard</a><a href="/services">Services</a>
<a href="/firewall">Firewall</a><a href="/vpn">VPN</a>
<a href="/system">System</a></nav>
<h1>User Management</h1>
<p>User management requires direct system access.</p>
<p>Use <code>aegisctl users list</code> from the system CLI.</p>
</body></html>"#.to_owned()
}

pub fn api_list() -> String {
    r#"{"users":[],"note":"user management via aegisctl CLI"}"#.to_owned()
}
