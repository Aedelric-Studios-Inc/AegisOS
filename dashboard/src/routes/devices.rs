//! Route handlers for /devices.

use crate::routes::render_page;
use crate::state::AppState;

pub fn index(state: &AppState) -> String {
    let rows = state
        .devices()
        .iter()
        .map(|device| {
            let roles = device.roles.join(", ");
            let exposure = device
                .open_ports
                .iter()
                .map(u16::to_string)
                .collect::<Vec<_>>()
                .join(", ");

            format!(
                r#"<tr>
  <td><strong>{}</strong><br><span class="muted">{}</span></td>
  <td>{}</td>
  <td class="{}">{}</td>
  <td>{}</td>
  <td>{}</td>
  <td>{}</td>
</tr>"#,
                device.hostname,
                roles,
                device.ip,
                device.risk.class_name(),
                device.risk.label(),
                if exposure.is_empty() {
                    "none".to_owned()
                } else {
                    exposure
                },
                device.reason,
                device.enforcement
            )
        })
        .collect::<Vec<_>>()
        .join("");

    let body = format!(
        r#"<section class="panel">
  <h2>Observed network clients</h2>
  <p class="muted" style="margin-top:10px;">Each address is classified so the appliance can quickly identify unknown public IPs, duplicate static leases, and exposed workloads.</p>
  <table>
    <thead>
      <tr><th>Device</th><th>IP address</th><th>Risk</th><th>Exposed ports</th><th>Decision reason</th><th>Enforcement</th></tr>
    </thead>
    <tbody>{}</tbody>
  </table>
</section>"#,
        rows
    );

    render_page("AegisOS Device Inventory", "/devices", body)
}
