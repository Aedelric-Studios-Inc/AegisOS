//! Route handlers for /dashboard.

use crate::routes::render_page;
use crate::state::AppState;

pub fn index(state: &AppState) -> String {
    let overview = state.overview();
    let service_cards = state
        .services()
        .iter()
        .map(|service| {
            format!(
                r#"<div class="service-card">
<div class="service-header">
  <div>
    <h3>{}</h3>
    <p class="muted">{}</p>
  </div>
  <div class="score">{}%</div>
</div>
<p class="{}">{}</p>
<ul><li>{}</li><li>{}</li></ul>
</div>"#,
                service.name,
                service.category,
                service.health_score,
                service.status_class(),
                service.status,
                service.summary,
                service.endpoint
            )
        })
        .collect::<Vec<_>>()
        .join("");

    let device_rows = state
        .devices()
        .iter()
        .take(4)
        .map(|device| {
            format!(
                "<tr><td>{}</td><td>{}</td><td class=\"{}\">{}</td><td>{}</td></tr>",
                device.hostname,
                device.ip,
                device.risk.class_name(),
                device.risk.label(),
                device.reason
            )
        })
        .collect::<Vec<_>>()
        .join("");

    let body = format!(
        r#"<section class="grid">
  <div class="panel"><p class="muted">Protected devices</p><p class="metric">{}</p></div>
  <div class="panel"><p class="muted">Blocked or quarantined IPs</p><p class="metric bad">{}</p></div>
  <div class="panel"><p class="muted">Monitored clients</p><p class="metric warn">{}</p></div>
  <div class="panel"><p class="muted">Platform services online</p><p class="metric good">{}</p></div>
</section>
<div class="split">
  <section class="panel">
    <div class="pill {}">{}</div>
    <h2>Network posture</h2>
    <p class="muted" style="margin-top:10px;">{}</p>
    <table>
      <thead><tr><th>Client</th><th>Address</th><th>Risk</th><th>Reason</th></tr></thead>
      <tbody>{}</tbody>
    </table>
  </section>
  <section class="panel">
    <div class="pill">{}</div>
    <h2>Priority actions</h2>
    <ul>
      <li>Quarantine IPs that advertise public or documentation-space addresses on the LAN.</li>
      <li>Reserve static leases for infrastructure nodes and alert on duplicate addresses.</li>
      <li>Keep VPN handshakes and Cloudflared tunnels pinned to known workloads.</li>
      <li>Use Docker and Nginx only behind the DNS and ad-block filtering chain.</li>
    </ul>
  </section>
</div>
<section class="panel" style="margin-top:18px;">
  <h2>Service board</h2>
  <p class="muted" style="margin-top:10px;">Critical services are summarized below. DNS filtering, VPN, static IP, and ad blocking use CPU-oriented assembly scoring paths on AArch64 builds.</p>
  <div class="stack" style="margin-top:18px;">{}</div>
</section>"#,
        overview.total_devices,
        overview.blocked_devices,
        overview.monitored_devices,
        overview.online_services,
        overview.network_risk.class_name(),
        overview.network_risk.label(),
        overview.summary,
        device_rows,
        overview.action_summary,
        service_cards
    );

    render_page("AegisOS Admin Dashboard", "/dashboard", body)
}
