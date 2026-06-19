//! auth route for rustmyadmin (login/logout).

pub fn login_page() -> String {
    r#"<!DOCTYPE html><html><head><meta charset="utf-8">
<title>AegisOS Admin — Login</title>
<style>body{font-family:monospace;background:#1a1a2e;color:#eee;display:flex;
align-items:center;justify-content:center;min-height:100vh;margin:0}
form{background:#0f3460;padding:2em;border-radius:8px;min-width:300px}
h1{color:#00d4ff;margin-top:0} input{width:100%;padding:.5em;margin:.4em 0 1em;
background:#1a1a2e;border:1px solid #00d4ff;color:#eee;border-radius:4px}
button{width:100%;padding:.7em;background:#00d4ff;color:#000;border:none;
border-radius:4px;cursor:pointer;font-weight:bold}</style>
</head><body>
<form method="POST" action="/auth/login">
<h1>AegisOS Admin</h1>
<label>Token<input type="password" name="token" autocomplete="current-password"></label>
<button type="submit">Sign In</button>
</form></body></html>"#.to_owned()
}
