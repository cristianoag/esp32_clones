# Firmware Change Log

This log tracks user-visible changes to the ESP32 TK95 / TK90X firmware.
Versions use one major digit and two minor digits, for example 1.00 and 1.01.

## 1.00 - Unreleased

### Added

- Added TK95 and 48 KiB TK90X emulation on the shared ESP32-S3 N16R8 CP400/MSX hardware.
- Added an F12 menu for ROM and machine selection, 50/60 Hz Microdigital timing, Portuguese/Spanish ROM selection, sound, volume, rendering interval and automatic boot.
- Added SD-loaded 16 KiB ROMs, standard TAP playback and 48K SNA/Z80 snapshot loading without embedding ROM data.
- Added dual USB joystick calibration, live input display and Kempston, Sinclair 1/2 and Cursor interface choices.
- Added separate TK boot defaults and joystick calibration storage, cold boot with or without saving, and warm reset with retained RAM.
- Added validated SD firmware updates, standalone build dependencies, native regression tests and the verified ESP32_TK95-1.00.FLH build package.

### Changed

- Allowed Firmware update to install compatible CP400, MSX and Apple II FLH packages as well as TK95 packages without renaming, while retaining image, checksum and OTA layout validation.
- Extended firmware-update host tests to install all four emulator packages into mock OTA storage and compare the written payloads.
- Moved the default SD ROM layout under the tk folder so TK and MSX files could share a card without mixing their BIOS directories.
- Migrated saved default ROM paths to the new TK layout while preserving custom ROM selections, tape paths and other settings.
- Changed keyboard and joystick UART diagnostic labels to identify the TK firmware instead of MSX.
- Updated firmware-update regression checks to validate generated TK firmware packages.

### Notes

- Limited the initial release to beeper audio, read-only standard TAP playback and loading plain 48K snapshots.
- Left AY, Betadisk, TZX, tape recording, snapshot saving and cycle-accuracy certification outside the initial release.
