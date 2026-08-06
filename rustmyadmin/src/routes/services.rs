//! Managed service view and actions.

use crate::aegisd_client::models::ServiceEntry;
use crate::aegisd_client::AegisdClient;

use super::html_escape;

pub fn index(csrf_token: &str) -> String {
    let raw = AegisdClient::query("/services");
    let rows = match &raw {
        Ok(json) if json.contains("\"error\":") => {
            "<tr><td colspan=7>Service manager is unavailable. See the raw response below.</td></tr>"
                .to_owned()
        }
        Ok(json) => match ServiceEntry::parse_list(json) {
            Ok(services) if services.is_empty() => {
                "<tr><td colspan=7>No managed services were returned.</td></tr>".to_owned()
            }
            Ok(services) => services
                .into_iter()
                .map(|service| {
                    let action = if service.active { "stop" } else { "start" };
                    let pid = service
                        .pid
                        .map(|value| value.to_string())
                        .unwrap_or_else(|| "-".to_owned());
                    let exit = service
                        .last_exit_code
                        .map(|value| value.to_string())
                        .unwrap_or_else(|| "-".to_owned());
                    format!(
                        r#"<tr><td>{name}</td><td>{state}</td><td>{pid}</td><td>{restarts}</td><td>{exit}</td><td>{critical}</td><td><form method="post" action="/services/{name}/{action}"><input type="hidden" name="csrf" value="{csrf}"><button>{action}</button></form></td></tr>"#,
                        name = html_escape(&service.name),
                        state = html_escape(&service.state),
                        restarts = service.restart_count,
                        critical = if service.critical { "yes" } else { "no" },
                        csrf = html_escape(csrf_token)
                    )
                })
                .collect::<String>(),
            Err(error) => format!(
                "<tr><td colspan=7>Could not parse service response: {}</td></tr>",
                html_escape(&error.to_string())
            ),
        },
        Err(error) => format!(
            "<tr><td colspan=7>Could not contact aegisd: {}</td></tr>",
            html_escape(&error.to_string())
        ),
    };
    let raw = raw.unwrap_or_else(|error| format!(r#"{{"error":"{}"}}"#, error));

    format!(
        r#"<!DOCTYPE html><html><head><meta charset="utf-8"><title>AegisOS Admin - Services</title>
<style>body{{font-family:monospace;background:#1a1a2e;color:#eee;padding:2em}}h1{{color:#00d4ff}}table{{border-collapse:collapse;width:100%}}th,td{{border:1px solid #31547e;padding:.65em;text-align:left}}pre{{background:#0f3460;padding:1em;overflow:auto}}a{{color:#00d4ff}}nav a{{margin-right:1em}}button{{padding:.45em .8em}}</style></head><body>
{nav}<h1>Service Management</h1><table><thead><tr><th>Service</th><th>State</th><th>PID</th><th>Restarts</th><th>Last exit</th><th>Critical</th><th>Action</th></tr></thead><tbody>{rows}</tbody></table><h2>Raw response</h2><pre>{raw}</pre></body></html>"#,
        nav = super::navigation(),
        raw = html_escape(&raw)
    )
}

pub fn api_list() -> String {
    AegisdClient::query("/services")
        .unwrap_or_else(|error| format!(r#"{{"error":"{}"}}"#, error))
}

pub fn change_state(name: &str, action: &str) -> Result<String, String> {
    if !valid_service_name(name) {
        return Err("invalid service name".to_owned());
    }
    if !matches!(action, "start" | "stop") {
        return Err("invalid service action".to_owned());
    }
    AegisdClient::post(&format!("/services/{name}/{action}"), "")
        .map_err(|error| error.to_string())
}

fn valid_service_name(name: &str) -> bool {
    !name.is_empty()
        && name.len() <= 80
        && name
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || matches!(byte, b'-' | b'_'))
}
