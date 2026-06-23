//! API surface for aegisd (re-exports route handlers).
#![allow(unused_imports)]

pub use crate::backups::list_backups;
pub use crate::containers::list_containers;
pub use crate::firewall::{add_rule, apply_baseline, delete_rule, list_rules};
pub use crate::network::{apply_network_profiles, get_interfaces, router_status};
pub use crate::router::{get_status as get_router_status, provision as provision_router};
pub use crate::hardening::{enforce_baseline, get_audit, get_baseline, get_incidents};
pub use crate::health::get_health;
pub use crate::logs::get_logs;
pub use crate::services::{list_services, start_service, stop_service};
pub use crate::updates::list_updates;
pub use crate::radio::{connect as radio_connect, disconnect as radio_disconnect, get_status as get_radio_status};
pub use crate::vpn::{down as vpn_down, get_status, up as vpn_up};
