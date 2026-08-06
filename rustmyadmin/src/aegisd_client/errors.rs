//! Errors returned by the aegisd client.

#[derive(Debug)]
pub enum AegisdError {
    Io(std::io::Error),
    Protocol(String),
}

impl std::fmt::Display for AegisdError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Io(error) => write!(f, "io: {error}"),
            Self::Protocol(message) => write!(f, "protocol: {message}"),
        }
    }
}

impl std::error::Error for AegisdError {}

impl From<std::io::Error> for AegisdError {
    fn from(error: std::io::Error) -> Self {
        Self::Io(error)
    }
}
