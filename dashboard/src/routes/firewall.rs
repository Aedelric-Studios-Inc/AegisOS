//! Route handlers for /firewall.

use crate::routes::render_page;
use crate::state::AppState;

pub fn index(state: &AppState) -> String {
    let services = state
        .services()
        .iter()
        .filter(|service| matches!(service.slug, "dns-filter" | "static-ip" | "ad-blocker"))
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
  <h2>Filtering and address control</h2>
  <p class="muted" style="margin-top:10px;">DNS filtering, static address verification, and ad-block control are the first line of defense for every host behind AegisBox.</p>
  <div class="stack" style="margin-top:18px;">{}</div>
</section>"#,
        services
    );

    render_page("AegisOS Filtering Controls", "/firewall", body)
}
