//! Persistent user management for RustMyAdmin.

use crate::auth::{password, roles::Role, sessions};
use crate::db;
use crate::db::models::UserRecord;

use super::{form_value, html_escape, json_escape, RequestContext};

pub fn index(context: &RequestContext<'_>) -> String {
    let users = db::pool::list_users();
    let rows = if users.is_empty() {
        "<tr><td colspan=4>No password-backed users are configured. Token login may still be active.</td></tr>".to_owned()
    } else {
        users
            .iter()
            .map(|user| {
                let delete = if context.role.can_manage_users() && user.username != "admin" {
                    format!(
                        r#"<form method="post" action="/users/{}/delete"><input type="hidden" name="csrf" value="{}"><button>Delete</button></form>"#,
                        html_escape(&user.username),
                        html_escape(context.csrf_token)
                    )
                } else {
                    "—".to_owned()
                };
                format!(
                    "<tr><td>{}</td><td>{}</td><td>{}</td><td>{}</td></tr>",
                    user.id,
                    html_escape(&user.username),
                    html_escape(&user.role),
                    delete
                )
            })
            .collect::<String>()
    };

    let create_form = if context.role.can_manage_users() {
        format!(
            r#"<h2>Create user</h2><form method="post" action="/users/create">
<input type="hidden" name="csrf" value="{}">
<label>Username <input name="username" required pattern="[A-Za-z0-9_-]+"></label>
<label>Password <input name="password" type="password" minlength="12" required></label>
<label>Role <select name="role"><option value="readonly">Read only</option><option value="operator">Operator</option><option value="admin">Admin</option></select></label>
<button>Create user</button></form>"#,
            html_escape(context.csrf_token)
        )
    } else {
        "<p>Your role cannot create or delete users.</p>".to_owned()
    };

    format!(
        r#"<!DOCTYPE html><html><head><meta charset="utf-8"><title>AegisOS Admin - Users</title>
<style>body{{font-family:monospace;background:#1a1a2e;color:#eee;padding:2em}}h1,h2{{color:#00d4ff}}table{{border-collapse:collapse;width:100%}}th,td{{border:1px solid #31547e;padding:.65em;text-align:left}}a{{color:#00d4ff}}nav a{{margin-right:1em}}form{{display:grid;gap:.7em;max-width:520px}}input,select,button{{padding:.55em}}</style></head><body>
{nav}<h1>User Management</h1><p>Signed in as <strong>{actor}</strong> ({role}).</p>
<table><thead><tr><th>ID</th><th>Username</th><th>Role</th><th>Action</th></tr></thead><tbody>{rows}</tbody></table>{create_form}</body></html>"#,
        nav = super::navigation(),
        actor = html_escape(context.username),
        role = context.role.as_str()
    )
}

pub fn api_list() -> String {
    let entries = db::pool::list_users()
        .into_iter()
        .map(|user| {
            format!(
                r#"{{"id":{},"username":"{}","role":"{}"}}"#,
                user.id,
                json_escape(&user.username),
                json_escape(&user.role)
            )
        })
        .collect::<Vec<_>>();
    format!(r#"{{"users":[{}]}}"#, entries.join(","))
}

pub fn create(body: &str) -> Result<String, String> {
    let username = form_value(body, "username").unwrap_or_default();
    let password_value = form_value(body, "password").unwrap_or_default();
    let role_value = form_value(body, "role").unwrap_or_else(|| "readonly".to_owned());

    if db::pool::load_user(&username).is_some() {
        return Err("a user with that username already exists".to_owned());
    }
    if password_value.len() < 12 {
        return Err("password must contain at least 12 characters".to_owned());
    }
    let role = Role::parse(&role_value).ok_or_else(|| "invalid role".to_owned())?;
    let record = UserRecord {
        id: db::pool::next_user_id(),
        username,
        password_hash: password::hash(&password_value)?,
        role: role.as_str().to_owned(),
    };
    db::pool::save_user(&record)?;
    Ok(format!(
        r#"{{"created":true,"id":{},"username":"{}","role":"{}"}}"#,
        record.id,
        json_escape(&record.username),
        record.role
    ))
}

pub fn delete(username: &str) -> Result<String, String> {
    if username == "admin" {
        return Err("the bootstrap admin account cannot be deleted".to_owned());
    }
    let removed = db::pool::delete_user(username)?;
    if removed {
        sessions::revoke_user(username);
    }
    Ok(format!(
        r#"{{"deleted":{},"username":"{}"}}"#,
        removed,
        json_escape(username)
    ))
}
