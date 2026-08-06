//! Typed data models returned by aegisd.

use super::errors::AegisdError;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct HealthResponse {
    pub status: String,
    pub uptime_seconds: u64,
}

impl HealthResponse {
    pub fn parse(json: &str) -> Result<Self, AegisdError> {
        Ok(Self {
            status: string_field(json, "status")?,
            uptime_seconds: u64_field(json, "uptime_seconds")?,
        })
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ServiceEntry {
    pub name: String,
    pub state: String,
    pub active: bool,
    pub pid: Option<u32>,
    pub restart_count: u64,
    pub last_exit_code: Option<i32>,
    pub critical: bool,
}

impl ServiceEntry {
    pub fn parse_list(json: &str) -> Result<Vec<Self>, AegisdError> {
        array_objects(json, "services")?
            .into_iter()
            .map(|object| {
                Ok(Self {
                    name: string_field(&object, "name")?,
                    state: string_field(&object, "state")?,
                    active: bool_field(&object, "active")?,
                    pid: optional_u32_field(&object, "pid")?,
                    restart_count: u64_field(&object, "restart_count")?,
                    last_exit_code: optional_i32_field(&object, "last_exit_code")?,
                    critical: bool_field(&object, "critical")?,
                })
            })
            .collect()
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FirewallRule {
    pub id: u32,
    pub chain: String,
    pub protocol: String,
    pub src: String,
    pub dst: String,
    pub port: Option<u16>,
    pub action: String,
}

impl FirewallRule {
    pub fn parse_list(json: &str) -> Result<Vec<Self>, AegisdError> {
        array_objects(json, "rules")?
            .into_iter()
            .map(|object| {
                let id = u32::try_from(u64_field(&object, "id")?).map_err(|_| {
                    AegisdError::Protocol("JSON field id exceeds u32".to_owned())
                })?;
                Ok(Self {
                    id,
                    chain: string_field(&object, "chain")?,
                    protocol: string_field(&object, "protocol")?,
                    src: string_field(&object, "src")?,
                    dst: string_field(&object, "dst")?,
                    port: optional_u16_field(&object, "port")?,
                    action: string_field(&object, "action")?,
                })
            })
            .collect()
    }
}

fn field_value<'a>(json: &'a str, key: &str) -> Result<&'a str, AegisdError> {
    let needle = format!("\"{key}\":");
    let start = json
        .find(&needle)
        .ok_or_else(|| AegisdError::Protocol(format!("missing JSON field {key}")))?
        + needle.len();
    Ok(json[start..].trim_start())
}

fn string_field(json: &str, key: &str) -> Result<String, AegisdError> {
    let rest = field_value(json, key)?;
    let inner = rest
        .strip_prefix('"')
        .ok_or_else(|| AegisdError::Protocol(format!("JSON field {key} is not a string")))?;

    let mut escaped = false;
    let mut output = String::new();
    for character in inner.chars() {
        if escaped {
            output.push(match character {
                'n' => '\n',
                'r' => '\r',
                't' => '\t',
                '"' => '"',
                '\\' => '\\',
                other => other,
            });
            escaped = false;
        } else if character == '\\' {
            escaped = true;
        } else if character == '"' {
            return Ok(output);
        } else {
            output.push(character);
        }
    }

    Err(AegisdError::Protocol(format!(
        "unterminated JSON string field {key}"
    )))
}

fn u64_field(json: &str, key: &str) -> Result<u64, AegisdError> {
    let rest = field_value(json, key)?;
    let end = rest
        .find(|character: char| !character.is_ascii_digit())
        .unwrap_or(rest.len());
    rest[..end]
        .parse::<u64>()
        .map_err(|_| AegisdError::Protocol(format!("JSON field {key} is not an integer")))
}

fn bool_field(json: &str, key: &str) -> Result<bool, AegisdError> {
    let rest = field_value(json, key)?;
    if rest.starts_with("true") {
        Ok(true)
    } else if rest.starts_with("false") {
        Ok(false)
    } else {
        Err(AegisdError::Protocol(format!(
            "JSON field {key} is not a boolean"
        )))
    }
}


fn optional_u32_field(json: &str, key: &str) -> Result<Option<u32>, AegisdError> {
    let rest = field_value(json, key)?;
    if rest.starts_with("null") {
        return Ok(None);
    }
    let value = u64_field(json, key)?;
    u32::try_from(value)
        .map(Some)
        .map_err(|_| AegisdError::Protocol(format!("JSON field {key} exceeds u32")))
}

fn optional_i32_field(json: &str, key: &str) -> Result<Option<i32>, AegisdError> {
    let rest = field_value(json, key)?;
    if rest.starts_with("null") {
        return Ok(None);
    }
    let end = rest
        .find(|character: char| character != '-' && !character.is_ascii_digit())
        .unwrap_or(rest.len());
    rest[..end]
        .parse::<i32>()
        .map(Some)
        .map_err(|_| AegisdError::Protocol(format!("JSON field {key} is not an i32")))
}

fn optional_u16_field(json: &str, key: &str) -> Result<Option<u16>, AegisdError> {
    let rest = field_value(json, key)?;
    if rest.starts_with("null") {
        return Ok(None);
    }
    let value = u64_field(json, key)?;
    u16::try_from(value)
        .map(Some)
        .map_err(|_| AegisdError::Protocol(format!("JSON field {key} exceeds u16")))
}

fn array_objects(json: &str, key: &str) -> Result<Vec<String>, AegisdError> {
    let needle = format!("\"{key}\":[");
    let start = json
        .find(&needle)
        .ok_or_else(|| AegisdError::Protocol(format!("missing JSON array {key}")))?
        + needle.len();

    let mut objects = Vec::new();
    let mut depth = 0_u32;
    let mut object_start = None;
    let mut in_string = false;
    let mut escaped = false;

    for (offset, byte) in json[start..].bytes().enumerate() {
        if in_string {
            if escaped {
                escaped = false;
            } else if byte == b'\\' {
                escaped = true;
            } else if byte == b'"' {
                in_string = false;
            }
            continue;
        }

        match byte {
            b'"' => in_string = true,
            b'{' => {
                if depth == 0 {
                    object_start = Some(start + offset);
                }
                depth += 1;
            }
            b'}' => {
                if depth == 0 {
                    return Err(AegisdError::Protocol(format!(
                        "unbalanced JSON object in {key}"
                    )));
                }
                depth -= 1;
                if depth == 0 {
                    let object_start = object_start.take().ok_or_else(|| {
                        AegisdError::Protocol(format!("missing object start in {key}"))
                    })?;
                    objects.push(json[object_start..=start + offset].to_owned());
                }
            }
            b']' if depth == 0 => return Ok(objects),
            _ => {}
        }
    }

    Err(AegisdError::Protocol(format!(
        "unterminated JSON array {key}"
    )))
}

#[cfg(test)]
mod tests {
    use super::{FirewallRule, HealthResponse, ServiceEntry};

    #[test]
    fn parses_health() {
        let health = HealthResponse::parse(r#"{"status":"ok","uptime_seconds":42}"#).unwrap();
        assert_eq!(health.status, "ok");
        assert_eq!(health.uptime_seconds, 42);
    }

    #[test]
    fn parses_services() {
        let services = ServiceEntry::parse_list(
            r#"{"services":[{"name":"firewall","state":"running","active":true,"pid":123,"restart_count":2,"last_exit_code":null,"critical":true}]}"#,
        )
        .unwrap();
        assert_eq!(services.len(), 1);
        assert_eq!(services[0].state, "running");
        assert_eq!(services[0].pid, Some(123));
        assert_eq!(services[0].restart_count, 2);
        assert_eq!(services[0].last_exit_code, None);
        assert!(services[0].critical);
    }

    #[test]
    fn parses_firewall_rules() {
        let rules = FirewallRule::parse_list(
            r#"{"rules":[{"id":1,"chain":"INPUT","protocol":"tcp","src":"any","dst":"any","port":443,"action":"ACCEPT"}]}"#,
        )
        .unwrap();
        assert_eq!(rules[0].port, Some(443));
    }
}
