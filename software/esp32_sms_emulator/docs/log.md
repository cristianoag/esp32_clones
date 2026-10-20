# Firmware Change Log

This log tracks user-visible changes to the Sega Master System firmware.
Versions use one major digit and two minor digits, for example 1.00 and 1.01.

## 1.00 - Unreleased

### Added

- Added a standalone Sega Master System firmware for the shared ESP32-S3 N16R8 board, with VGA graphics, SN76489 audio and NTSC/PAL console timing.
- Added the F12 menu with cartridge loading, configuration, console Pause, reset, saved boot defaults, hardware help and firmware updates.
- Added a paged SD browser for SMS and ROM cartridges, nested directories and optional 512-byte copier headers, without requiring a BIOS.
- Added automatic cartridge mapper selection and manual Sega, Codemasters and Korean mapper choices.
- Added two independently enabled USB joystick ports with connection status, seven-step calibration, saved controller mappings and keyboard controls for both players.
- Added the shared dual-slot OTA layout and verified FLH firmware packaging so users could switch between current emulator builds without renaming packages.
- Added host regression tests for cartridge execution, audio/video output, F12 navigation, ROM validation, joystick input, settings, PWM audio and firmware updates.

### Notes

- Cartridge SRAM remained session-only; FM audio, save states, Game Gear and SG-1000 modes were not included.
- Physical-board speed, VGA/audio quality, real USB devices and live firmware switching still required hardware validation.
