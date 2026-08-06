# RustMyAdmin Integration

AegisOS exposes a RustMyAdmin-style administrative surface for inspecting appliance state.

## Current implementation

- The live read-only experience is embedded in `dashboard/`.
- HTML routes live under `dashboard/src/routes/rustmyadmin.rs`.
- JSON APIs are exposed from `dashboard/src/api.rs`.

## Target layout

The dedicated `rustmyadmin/` tree in this repository provides a standalone home for templates, static assets, database helpers, authentication, and security middleware when that surface is extracted from the dashboard crate.
