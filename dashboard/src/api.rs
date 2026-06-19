//! REST API handlers.

use crate::state::AppState;

pub fn get_status(state: &AppState) -> String {
    state.status_json()
}

pub fn get_devices(state: &AppState) -> String {
    state.devices_json()
}
