# Firmware Change Log

This log tracks user-visible changes to the Sega Master System firmware.
Versions use one major digit and two minor digits, for example 1.00 and 1.01.

## 1.03 - Unreleased

### Fixed

- Reduced the time to open Load ROM with hundreds of cartridges by reading directory names and types without opening each file.
- Removed repeated microSD scans from arrow-key and page navigation by keeping the current folder's list in PSRAM, while refreshing it when changing folders or reopening the browser.
- Reported directory read and list-allocation failures instead of showing an incomplete cartridge list.
- Improved emulation throughput by keeping CPU, working RAM and VDP state in internal SRAM when available, while leaving large tile-cache and cartridge-SRAM buffers separately allocated and preserving failed-load recovery.
- Reduced CPU memory-access overhead and VGA scaling work without changing console timing, pixel output or the selected frame-skipping setting.
- Stopped rewriting unchanged VGA borders every frame while retaining complete screen restoration after F12 and video-mode changes.
- Reduced rendering memory traffic by preferring internal SRAM for the game framebuffer when enough room remained for peripherals and a staged cartridge replacement.
- Reduced VGA transfer overhead by scaling each output row in SRAM before copying it to the VGA buffer in a single bulk transfer.
- Accelerated fully visible background tiles while preserving scrolling, clipping, flips, palette priority and sprite collision results.

### Added

- Added UART reports of core and framebuffer memory placement and average emulation, VGA and audio submission times to help diagnose remaining frame-rate limits on the board.

## 1.02 - Unreleased

### Changed

- Changed the parent-folder `make` and `make firmware` commands to clear FLH files from the shared SD-card root and copy only the latest Sega Master System package there, leaving emulator project roots and build archives unchanged.

### Fixed

- Applied the MSX-tested USB receive fix so brief SE0/SE1 transitions no longer ended joystick descriptor packets early and prevented detection.
- Preserved receive-edge timestamps and ACK timing while retaining CRC validation, saved calibration and both players' enable settings.
- Added receive-edge traces for the first two failed control transactions and timing/packet regressions for both connectors.

## 1.01 - Unreleased

### Added

- Copied the latest prebuilt Sega Master System firmware package to this emulator's project root and the shared SD-card root when running `make` or `make firmware` from the parent software folder, without rebuilding firmware.

### Fixed

- Restored low-speed USB joystick compatibility by starting raw calibrated reports without requiring a short, recognized HID report descriptor.
- Requested HID idle mode on the selected interface as CP400 did, while allowing controllers that rejected this optional request to continue sending input.
- Preserved both players' enable settings and saved calibration while keeping uncalibrated and disconnected controllers neutral.

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
