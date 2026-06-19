pub mod dashboard;
pub mod devices;
pub mod firewall;
pub mod logs;
pub mod updates;
pub mod vpn;

pub(crate) fn render_page(title: &str, active: &str, body: String) -> String {
    let navigation = [
        ("/dashboard", "Overview"),
        ("/devices", "Devices"),
        ("/firewall", "Filtering"),
        ("/vpn", "VPN"),
        ("/logs", "Logs"),
        ("/updates", "Platform"),
    ]
    .into_iter()
    .map(|(path, label)| {
        let current = if path == active { "active" } else { "" };
        format!(r#"<a class="nav-link {current}" href="{path}">{label}</a>"#)
    })
    .collect::<Vec<_>>()
    .join("");

    format!(
        r#"<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{title}</title>
  <style>
    :root {{
      color-scheme: dark;
      --bg: #08111f;
      --panel: #0f1b2d;
      --border: #1f3557;
      --text: #e8edf7;
      --muted: #92a7c7;
      --accent: #56b4ff;
      --good: #27d07d;
      --warn: #ffb020;
      --bad: #ff5d73;
    }}
    * {{ box-sizing: border-box; }}
    body {{
      margin: 0;
      font-family: Inter, system-ui, sans-serif;
      background: linear-gradient(180deg, #07101d 0%, #091728 100%);
      color: var(--text);
    }}
    header {{
      padding: 24px 32px 12px;
      border-bottom: 1px solid var(--border);
      background: rgba(9, 23, 40, 0.92);
      position: sticky;
      top: 0;
      backdrop-filter: blur(12px);
    }}
    h1, h2, h3, p {{ margin: 0; }}
    h1 {{ font-size: 1.8rem; }}
    .subtitle {{ color: var(--muted); margin-top: 6px; }}
    nav {{ display: flex; gap: 10px; flex-wrap: wrap; margin-top: 18px; }}
    .nav-link {{
      color: var(--muted);
      text-decoration: none;
      border: 1px solid var(--border);
      border-radius: 999px;
      padding: 8px 14px;
      background: rgba(15, 27, 45, 0.75);
    }}
    .nav-link.active {{
      color: var(--text);
      border-color: var(--accent);
      background: rgba(86, 180, 255, 0.14);
    }}
    main {{ padding: 28px 32px 40px; }}
    .grid {{
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
      gap: 18px;
    }}
    .panel {{
      background: rgba(15, 27, 45, 0.92);
      border: 1px solid var(--border);
      border-radius: 18px;
      padding: 20px;
      box-shadow: 0 14px 28px rgba(0, 0, 0, 0.2);
    }}
    .metric {{ font-size: 2rem; margin-top: 10px; }}
    .muted {{ color: var(--muted); }}
    .good {{ color: var(--good); }}
    .warn {{ color: var(--warn); }}
    .bad {{ color: var(--bad); }}
    .pill {{
      display: inline-flex;
      align-items: center;
      gap: 6px;
      padding: 4px 10px;
      border-radius: 999px;
      font-size: 0.8rem;
      border: 1px solid var(--border);
      margin-bottom: 12px;
    }}
    table {{
      width: 100%;
      border-collapse: collapse;
      margin-top: 16px;
    }}
    th, td {{
      text-align: left;
      padding: 12px 10px;
      border-bottom: 1px solid rgba(31, 53, 87, 0.7);
      vertical-align: top;
    }}
    th {{ color: var(--muted); font-weight: 600; }}
    .stack {{ display: grid; gap: 14px; }}
    .service-card {{
      padding: 18px;
      border-radius: 16px;
      border: 1px solid rgba(31, 53, 87, 0.8);
      background: rgba(8, 17, 31, 0.65);
    }}
    .service-header {{
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 12px;
      flex-wrap: wrap;
    }}
    .score {{
      font-weight: 700;
      color: var(--accent);
    }}
    ul {{ margin: 12px 0 0 18px; color: var(--muted); }}
    .split {{
      display: grid;
      grid-template-columns: 1.2fr 0.8fr;
      gap: 18px;
      margin-top: 18px;
    }}
    @media (max-width: 900px) {{
      header, main {{ padding-left: 18px; padding-right: 18px; }}
      .split {{ grid-template-columns: 1fr; }}
    }}
  </style>
</head>
<body>
  <header>
    <h1>{title}</h1>
    <p class="subtitle">Pi-hole-style network control plane for AegisBox appliances.</p>
    <nav>{navigation}</nav>
  </header>
  <main>{body}</main>
</body>
</html>"#
    )
}
