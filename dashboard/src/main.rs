//! AegisOS Dashboard — web management interface.

mod state;
mod auth;
mod api;
mod routes;

fn main() {
    println!("[dashboard] AegisOS Dashboard starting on http://0.0.0.0:8080");
    // Initialize app state
    let _state = state::AppState::new();
    // TODO: start HTTP server, mount routes
}
