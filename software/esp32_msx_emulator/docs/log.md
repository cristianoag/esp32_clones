# Firmware Change Log

This log tracks user-visible changes to the ESP32 MSX / fMSX firmware.
Versions use one major digit and two minor digits, for example 1.00 and 1.01.

## 1.00 - Unreleased

### Added

- Added an independent, non-commercial fMSX-based MSX1/MSX2/MSX2+ firmware for the shared ESP32-S3 N16R8 board.
- Matched the existing board's VGA, native USB keyboard, one-bit SD/MMC, and GPIO47 mono audio wiring, and reserved GPIO48 for the onboard LED.
- Added an F12 configuration menu that paused and resumed emulation, selected BIOS profiles and RAM sizes, controlled sound and automatic boot, saved boot defaults, and cold-booted another selected ROM.
- Added active-low MSX keyboard input from a directly connected USB boot keyboard, reserved F12 for configuration, rejected rollover reports, and released held keys on disconnect.
- Added initial Omega MSX2+ NTSC, Gradiente Expert 1.1, and Sharp Hotbit 1.2 BIOS profiles with SD-card ROM loading and explicit errors for missing or malformed files.
- Added a local ROM importer that selected either 256 KiB Omega bank, defaulted to the first bank, extracted the correct main and extension BIOS regions, and generated copy-verification checksums without embedding ROMs in firmware.
- Added custom MSX1, MSX2, and MSX2+ boot profiles discoverable from SD without rebuilding firmware.
- Added independent PlatformIO and make build targets and ROM-import regression checks.
- Added CP400-compatible FLH packaging in the dist folder through `make` and `make firmware`, with checksum and payload verification, and removed the selected package through `make clean`.
- Added SD-card cartridge ROM selection and ejection for emulated slots 1 and 2, applied the selected cartridges on cold boot, and saved both assignments with the boot defaults while preserving older settings.
- Added a paginated folder browser for cartridge ROMs and firmware packages, with case-insensitive extension matching and explicit errors for invalid paths or files.
- Added an F12 firmware-update flow with confirmation, progress, FLH validation, inactive-partition installation, and automatic reboot after success.

### Changed

- Placed F12 options on consecutive text rows without blank lines between them.

### Fixed

- Eliminated repeated Library Manager manifest parsing errors when building with the shared VGA library.

### Know issues with this version

- Audio quality is not good, especially at higher sample rates.
- The emulator is a bit slow when compared to the real Z-80
- The firmware does not yet support all MSX hardware features, such as disk drives.

### Notes

- Required user-supplied BIOS images and retained the original fMSX author's non-commercial terms and attribution.
- Used fMSX's generic machine models rather than emulating the physical Omega board's optional flash, logo, disk, and expansion hardware.
- Left disk and tape browsing, joystick ports, and save states outside the initial release.
