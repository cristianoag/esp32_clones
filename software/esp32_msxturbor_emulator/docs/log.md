# Firmware Change Log

This log tracks user-visible changes to the ESP32 MSX TurboR / FS-A1ST firmware.
Versions use one major digit and two minor digits, for example 1.00 and 1.01.

## 1.03 - Unreleased

### Changed

- Changed the parent-folder `make` and `make firmware` commands to clear FLH files from the shared SD-card root and copy only the latest MSX TurboR package there, leaving emulator project roots and build archives unchanged.

### Fixed

- Applied the MSX-tested USB receive fix so brief SE0/SE1 transitions no longer ended joystick descriptor packets early and prevented detection.
- Preserved receive-edge timestamps and ACK timing while retaining CRC validation, both MSX ports and TurboR-specific saved calibration.
- Added receive-edge traces for the first two failed control transactions and extended timing/packet regressions across both connectors.

## 1.02 - Unreleased

### Added

- Copied the latest prebuilt MSX TurboR firmware package to this emulator's project root and the shared SD-card root when running `make` or `make firmware` from the parent software folder, without rebuilding firmware.
- Added two independent low-speed USB joystick ports on GPIO15/16 and GPIO17/18 and connected their directions and two buttons to the emulated MSX PSG ports.
- Added an F12 USB joysticks page with live input, seven-step calibration, per-controller saved mappings and a clear-calibration action.
- Stored calibration separately from normal MSX and kept uncalibrated or disconnected controllers neutral.

### Fixed

- Matched CP400's raw-report compatibility and HID idle setup without requiring a short, recognized HID report descriptor.
- Paused and reset the software USB host around settings, calibration and firmware flash writes so input resumed safely after saving or a failed update.

## 1.01 - Unreleased

### Added

- Added a Media menu with two cartridge ROM slots, the same Auto/manual mapper choices as normal MSX, SCC sound, and persistent GameMaster2/FM-PAC SRAM.
- Added a second writable floppy drive and independent attachment/ejection for drives A and B, while retaining an existing disk if its replacement failed to open.
- Added a saved VGA fit setting from 80% to 100%, defaulting to a centered 90% safe area for both menus and the emulated picture on monitors that clipped the edges.
- Migrated existing saved A1ST boot settings while preserving Drive A, sound and other boot preferences.

### Fixed

- Changed the microSD clock from the library's 40 MHz default to the same 20 MHz used by the other Clone Series firmwares to avoid card initialization and read failures.
- Removed unnecessary ROM-file seeks and buffered reads during A1ST startup, and handled short and interrupted reads while loading into PSRAM.
- Replaced misleading ROM-size errors on read failures with the failed operation, byte offset and OS error in the startup message and UART diagnostic.

## 1.00 - Unreleased

### Added

- Added a standalone Panasonic FS-A1ST firmware using the blueMSX core and user-supplied original firmware and Kanji ROMs.
- Added Z80/R800 switching, DRAM mode, 256 KiB RAM, V9958 video, Kanji, PSG, MSX-MUSIC and TurboR PCM output.
- Added the original TurboR disk-controller interface with one writable raw 360/720 KiB disk drive and synchronized sector writes.
- Added the shared VGA wiring, native USB keyboard, SD/MMC storage and mono PWM audio.
- Added an F12 configuration menu with disk browsing/ejection, sound, firmware switch, automatic boot, presentation rate and saved boot settings.
- Added checked firmware SRAM and CMOS persistence when entering F12 or rebooting through the menu.
- Added the shared dual-slot FLH updater so current Clone Series firmwares could install the TurboR build and switch back without renaming packages.
- Added validated ROM importing, standalone build/package commands, host regressions and reference-code license documentation.

### Notes

- Physical-board speed, video/audio quality and firmware switching remained unvalidated; the initial release did not claim full-speed or cycle-perfect emulation.
- Cartridge/tape selection, USB joysticks, microphone recording and A1GT/MIDI were not included.
