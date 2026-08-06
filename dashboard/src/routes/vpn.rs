//! Route handlers for /vpn.

use crate::routes::render_page;
use crate::state::AppState;

pub fn index(state: &AppState) -> String {
    let services = state
        .services()
        .iter()
        .filter(|service| matches!(service.slug, "vpn" | "cloudflare-tunnel"))
        .map(|service| {
            format!(
                r#"<div class="service-card">
  <div class="service-header"><h3>{}</h3><span class="score">{}%</span></div>
  <p class="{}">{}</p>
  <ul><li>{}</li><li>{}</li></ul>
</div>"#,
                service.name,
                service.health_score,
                service.status_class(),
                service.status,
                service.summary,
                service.endpoint
            )
        })
        .collect::<Vec<_>>()
        .join("");

    let body = format!(
        r#"<section class="panel">
  <h2>Remote access and tunnels</h2>
  <p class="muted" style="margin-top:10px;">VPN handshakes and Cloudflared exposure should stay pinned to trusted endpoints only.</p>
  <div class="stack" style="margin-top:18px;">{}</div>
</section>"#,
        services
    );

    render_page("AegisOS VPN and Tunnels", "/vpn", body)
}
