//! REST API handlers.

use crate::state::AppState;

pub fn get_status(state: &AppState) -> String {
    state.status_json()
}

pub fn get_devices(state: &AppState) -> String {
    state.devices_json()
}

pub fn get_rustmyadmin_catalog(state: &AppState) -> String {
    state.rustmyadmin_catalog_json()
}

pub fn get_rustmyadmin_table(
    state: &AppState,
    database_slug: &str,
    table_slug: &str,
) -> Option<String> {
    state.rustmyadmin_table_json(database_slug, table_slug)
}
