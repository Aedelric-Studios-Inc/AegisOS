//! REST API handlers.

pub fn get_status() -> &'static str {
    r#"{"status":"ok","version":"0.1.0"}"#
}
