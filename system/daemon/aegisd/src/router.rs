//! Router orchestration endpoints.

use crate::{dns, firewall, network, radio, vpn};

pub fn get_status() -> String {
    format!(
        r#"{{"router_status":{{"network":{},"radio":{},"firewall":{},"dns":{},"vpn":{}}}}}"#,
        strip_outer(network::router_status(), "router"),
        strip_outer(radio::get_status(), "radio"),
        strip_outer(firewall::status(), "firewall"),
        strip_outer(dns::status(), "dns"),
        vpn::status_object(),
    )
}

pub fn provision() -> String {
    let network = network::apply_network_profiles();
    let firewall = firewall::apply_baseline();
    let dns = dns::write_dnsmasq_config();
    format!(
        r#"{{"provision":{{"network":{},"firewall":{},"dns":{}}}}}"#,
        strip_outer(network, "network_apply"),
        strip_outer(firewall, "firewall_apply"),
        strip_outer(dns, "dnsmasq_config"),
    )
}

fn strip_outer(json: String, key: &str) -> String {
    let prefix = format!(r#"{{"{}":"#, key);
    if let Some(rest) = json.strip_prefix(&prefix) {
        if let Some(inner) = rest.strip_suffix('}') {
            return inner.to_owned();
        }
    }
    json
}
