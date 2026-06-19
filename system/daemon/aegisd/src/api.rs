//! API surface for aegisd (re-exports route handlers).

pub use crate::health::get_health;
pub use crate::services::{list_services, start_service, stop_service};
pub use crate::firewall::{list_rules, add_rule, delete_rule};
pub use crate::logs::get_logs;
pub use crate::vpn::get_status;
pub use crate::updates::list_updates;
pub use crate::containers::list_containers;
pub use crate::backups::list_backups;
