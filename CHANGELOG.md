# Changelog

## v0.1.0 — 2026-11-17

- Initial release of standalone ESP32-C5 one-shot and held-button lab beacon transmitter.
- D6 cycles channels 6/11/1; D7 cycles requested TX power 2/8/15 dBm; D5 triggers transmissions.
- Nonblocking active-low onboard LED acknowledgments: startup ready lamp, 1–3 short channel flashes, 1–3 longer power flashes, brief TX blip, and idle heartbeat.
- Configuration indications supersede TX blips; no blocking delays are introduced for ordinary operation.
- Restore channel 6 / requested 2 dBm on each reboot; intentional, no power persistence.
- Fix GCC misleading-indentation failure in NVS initialization.
- MIT license and full documentation.
