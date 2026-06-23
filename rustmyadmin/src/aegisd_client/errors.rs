//! aegisd client errors.

#[derive(Debug)]
pub enum AegisdError {
    Io(std::io::Error),
    Protocol(String),
}

impl std::fmt::Display for AegisdError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            AegisdError::Io(e)         => write!(f, "io: {}", e),
            AegisdError::Protocol(msg) => write!(f, "protocol: {}", msg),
        }
    }
}

impl From<std::io::Error> for AegisdError {
    fn from(e: std::io::Error) -> Self { AegisdError::Io(e) }
}
