//! Browser login page for RustMyAdmin.

use super::html_escape;

pub fn login_page(error: Option<&str>, configured: bool) -> String {
    let error_html = error
        .map(|message| format!(r#"<div class="error">{}</div>"#, html_escape(message)))
        .unwrap_or_default();
    let setup_html = if configured {
        String::new()
    } else {
        r#"<div class="error">No authentication secret is configured. Set RUSTMYADMIN_ADMIN_PASSWORD, RUSTMYADMIN_ADMIN_PASSWORD_HASH, or RUSTMYADMIN_TOKEN before starting RustMyAdmin.</div>"#.to_owned()
    };

    format!(
        r#"<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>AegisOS Admin — Login</title>
<style>
:root{{color-scheme:dark}}*{{box-sizing:border-box}}body{{font-family:system-ui,sans-serif;background:#08111f;color:#eef4ff;display:grid;place-items:center;min-height:100vh;margin:0;padding:24px}}
form{{background:#0f1b2d;border:1px solid #24446d;padding:28px;border-radius:20px;width:min(420px,100%);box-shadow:0 24px 60px #0008}}
h1{{color:#61c4ff;margin:0 0 8px}}p{{color:#a9bad2}}label{{display:block;margin-top:14px;color:#c8d4e5}}input,select{{width:100%;padding:12px;margin-top:7px;background:#08111f;border:1px solid #31547e;color:#eef4ff;border-radius:10px}}
button{{width:100%;padding:12px;margin-top:18px;background:#61c4ff;color:#06101d;border:0;border-radius:10px;font-weight:800;cursor:pointer}}.error{{background:#431b28;border:1px solid #a84960;color:#ffdbe3;padding:12px;border-radius:10px;margin-top:14px}}
</style></head><body><form method="POST" action="/auth/login">
<h1>RustMyAdmin</h1><p>Authenticate to manage AegisOS services and system configuration.</p>
{setup_html}{error_html}
<label>Username<input name="username" value="admin" autocomplete="username" required></label>
<label>Password or admin token<input type="password" name="secret" autocomplete="current-password" required></label>
<button type="submit">Sign in</button></form></body></html>"#
    )
}
