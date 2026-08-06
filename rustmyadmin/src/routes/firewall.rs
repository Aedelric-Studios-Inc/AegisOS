//! Firewall status and rule rendering.

use crate::aegisd_client::models::FirewallRule;
use crate::aegisd_client::AegisdClient;

use super::html_escape;

pub fn index() -> String {
    let raw = AegisdClient::query("/firewall/rules");
    let rows = match &raw {
        Ok(json) => match FirewallRule::parse_list(json) {
            Ok(rules) if rules.is_empty() => {
                "<tr><td colspan=7>No runtime firewall rules are registered.</td></tr>".to_owned()
            }
            Ok(rules) => rules
                .into_iter()
                .map(|rule| {
                    format!(
                        "<tr><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td></tr>",
                        rule.id,
                        html_escape(&rule.chain),
                        html_escape(&rule.protocol),
                        html_escape(&rule.src),
                        html_escape(&rule.dst),
                        rule.port.map(|port| port.to_string()).unwrap_or_else(|| "any".to_owned()),
                        html_escape(&rule.action)
                    )
                })
                .collect::<String>(),
            Err(error) => format!(
                "<tr><td colspan=7>Could not parse firewall response: {}</td></tr>",
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
        r#"<!DOCTYPE html><html><head><meta charset="utf-8"><title>AegisOS Admin - Firewall</title>
<style>body{{font-family:monospace;background:#1a1a2e;color:#eee;padding:2em}}h1{{color:#00d4ff}}table{{border-collapse:collapse;width:100%}}th,td{{border:1px solid #31547e;padding:.65em;text-align:left}}pre{{background:#0f3460;padding:1em;overflow:auto}}a{{color:#00d4ff}}nav a{{margin-right:1em}}</style></head><body>
{nav}<h1>Firewall Rules</h1><table><thead><tr><th>ID</th><th>Chain</th><th>Protocol</th><th>Source</th><th>Destination</th><th>Port</th><th>Action</th></tr></thead><tbody>{rows}</tbody></table><h2>Raw response</h2><pre>{raw}</pre></body></html>"#,
        nav = super::navigation(),
        raw = html_escape(&raw)
    )
}

pub fn api_list() -> String {
    AegisdClient::query("/firewall/rules")
        .unwrap_or_else(|error| format!(r#"{{"error":"{}"}}"#, error))
}
