//! Shared application state.

pub struct AppState {
    pub version: &'static str,
}

impl AppState {
    pub fn new() -> Self {
        Self { version: env!("CARGO_PKG_VERSION") }
    }
}
