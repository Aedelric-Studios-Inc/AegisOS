//! Route handlers for /rustmyadmin.

use crate::routes::render_page;
use crate::state::{AdminColumn, AppState};

pub fn index(state: &AppState) -> String {
    let databases = state
        .admin_databases()
        .iter()
        .map(|database| {
            let table_rows = database
                .tables
                .iter()
                .map(|table| {
                    format!(
                        r#"<tr>
  <td><a href="/rustmyadmin/{}/table/{}">{}</a></td>
  <td>{}</td>
  <td>{}</td>
  <td><code>{}</code></td>
</tr>"#,
                        database.slug,
                        table.slug,
                        escape_html(table.name),
                        table.rows,
                        escape_html(table.engine),
                        escape_html(table.recommended_query)
                    )
                })
                .collect::<Vec<_>>()
                .join("");

            format!(
                r#"<section class="panel">
  <div class="panel-heading">
    <div>
      <p class="muted">Database</p>
      <h2>{}</h2>
    </div>
    <div class="pill">{}</div>
  </div>
  <p class="muted" style="margin-top:10px;">{}</p>
  <div class="chip-row" style="margin-top:14px;">
    <span class="chip">Engine: {}</span>
    <span class="chip">Access: {}</span>
    <span class="chip">Tables: {}</span>
  </div>
  <table>
    <thead><tr><th>Table</th><th>Rows</th><th>Engine</th><th>Read-only query</th></tr></thead>
    <tbody>{}</tbody>
  </table>
</section>"#,
                escape_html(database.name),
                escape_html(database.slug),
                escape_html(&database.summary),
                escape_html(database.engine),
                escape_html(database.access),
                database.tables.len(),
                table_rows
            )
        })
        .collect::<Vec<_>>()
        .join("");

    let total_tables = state
        .admin_databases()
        .iter()
        .map(|database| database.tables.len())
        .sum::<usize>();

    let body = format!(
        r#"<section class="grid">
  <div class="panel"><p class="muted">Integrated surface</p><p class="metric good">RustMyAdmin</p></div>
  <div class="panel"><p class="muted">Database groups</p><p class="metric">{}</p></div>
  <div class="panel"><p class="muted">Browsable tables</p><p class="metric">{}</p></div>
  <div class="panel"><p class="muted">SQL mode</p><p class="metric warn">Read only</p></div>
</section>
<section class="panel" style="margin-top:18px;">
  <div class="panel-heading">
    <div>
      <h2>RustMyAdmin inside Dashboard</h2>
      <p class="muted" style="margin-top:8px;">This embedded view brings a phpMyAdmin-style schema browser into the AegisOS dashboard without adding a separate web stack.</p>
    </div>
    <div class="pill good">Owner-ready</div>
  </div>
  <ul>
    <li>Browse synthetic AegisOS databases that mirror device, service, and audit records.</li>
    <li>Inspect table structure, sample rows, and a safe SELECT statement for each table.</li>
    <li>Keep the interface read-only so dashboard operators can review state without mutating platform data.</li>
  </ul>
</section>
<div class="stack" style="margin-top:18px;">{}</div>"#,
        state.admin_databases().len(),
        total_tables,
        databases
    );

    render_page("AegisOS Dashboard · RustMyAdmin", "/rustmyadmin", body)
}

pub fn table_detail(state: &AppState, database_slug: &str, table_slug: &str) -> Option<String> {
    let (database, table) = state.find_admin_table(database_slug, table_slug)?;
    let column_rows = table
        .columns
        .iter()
        .map(render_column_row)
        .collect::<Vec<_>>()
        .join("");
    let sample_headers = table
        .columns
        .iter()
        .map(|column| format!("<th>{}</th>", escape_html(column.name)))
        .collect::<Vec<_>>()
        .join("");
    let sample_rows = table
        .sample_rows
        .iter()
        .map(|row| {
            let cells = row
                .iter()
                .map(|value| format!("<td>{}</td>", escape_html(value)))
                .collect::<Vec<_>>()
                .join("");
            format!("<tr>{cells}</tr>")
        })
        .collect::<Vec<_>>()
        .join("");

    let body = format!(
        r#"<section class="panel">
  <div class="panel-heading">
    <div>
      <p class="muted">Database / Table</p>
      <h2>{}.{} </h2>
    </div>
    <a class="nav-link" href="/rustmyadmin">Back to catalog</a>
  </div>
  <p class="muted" style="margin-top:10px;">{}</p>
  <div class="chip-row" style="margin-top:14px;">
    <span class="chip">Rows: {}</span>
    <span class="chip">Engine: {}</span>
    <span class="chip">Access: {}</span>
  </div>
</section>
<div class="split" style="margin-top:18px;">
  <section class="panel">
    <h2>Structure</h2>
    <table>
      <thead><tr><th>Column</th><th>Type</th><th>Nullable</th><th>Key</th></tr></thead>
      <tbody>{}</tbody>
    </table>
  </section>
  <section class="panel">
    <h2>Safe query preview</h2>
    <p class="muted" style="margin-top:10px;">RustMyAdmin stays read-only in dashboard mode. Use the suggested SELECT to inspect state without mutating it.</p>
    <pre><code>{}</code></pre>
    <p class="muted" style="margin-top:14px;">Blocked verbs: INSERT, UPDATE, DELETE, ALTER, DROP, TRUNCATE.</p>
  </section>
</div>
<section class="panel" style="margin-top:18px;">
  <h2>Sample rows</h2>
  <div style="overflow-x:auto;">
    <table>
      <thead><tr>{}</tr></thead>
      <tbody>{}</tbody>
    </table>
  </div>
</section>"#,
        escape_html(database.name),
        escape_html(table.name),
        escape_html(&table.note),
        table.rows,
        escape_html(table.engine),
        escape_html(database.access),
        column_rows,
        escape_html(table.recommended_query),
        sample_headers,
        sample_rows
    );

    Some(render_page(
        "AegisOS Dashboard · RustMyAdmin Table",
        "/rustmyadmin",
        body,
    ))
}

fn render_column_row(column: &AdminColumn) -> String {
    format!(
        "<tr><td>{}</td><td>{}</td><td>{}</td><td>{}</td></tr>",
        escape_html(column.name),
        escape_html(column.data_type),
        if column.nullable { "YES" } else { "NO" },
        if column.key.is_empty() {
            "&mdash;".to_owned()
        } else {
            escape_html(column.key)
        }
    )
}

fn escape_html(value: &str) -> String {
    value
        .replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('"', "&quot;")
        .replace('\'', "&#39;")
}
