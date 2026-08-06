# PhoenixOS Preparation Layer

PhoenixOS is not the current production target. It is the future phone/mobile OS
built around the AegisOS kernel/core foundation.

This directory exists to keep AegisOS core clean: do not put phone-only UX,
carrier certification logic, camera HAL work, or app-store assumptions directly
inside the AegisBox edition.

## Reusable from AegisBox/AegisOS

- boot/update/recovery model
- daemon/control-plane design
- audit/event model
- radio abstraction concepts
- networking and firewall policy model
- payment/account/licence integration points
- RustMyAdmin/device-management integration points

## Deferred PhoenixOS-specific work

- mobile shell and launcher
- app runtime and app permissions UX
- calls/SMS/MMS
- VoLTE/IMS and emergency calling
- camera/media HAL
- battery/power-management UX
- RF/SAR and carrier certification path
