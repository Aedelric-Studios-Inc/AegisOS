# PhoenixOS Radio Contract Draft

This contract is deliberately small. AegisBox proves data networking first;
PhoenixOS later expands the same concept for phone-only features.

## AegisBox-proven fields

- modem present/absent
- SIM present/absent
- carrier profile name
- APN profile
- registration state
- signal quality
- data bearer state
- IP address
- failover state
- health/error state

## PhoenixOS-only extensions later

- voice registration
- SMS/MMS state
- VoLTE/IMS state
- emergency calling support
- eSIM profile management
- per-app mobile-data permissions
