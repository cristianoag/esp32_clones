# Firmware Change Log

This log tracks user-visible changes to the Commodore 64 firmware.
Versions use one major digit and two minor digits, for example 1.00 and 1.01.

## 1.02 - Unreleased

### Changed

- Changed the parent-folder `make` and `make firmware` commands to clear FLH files from the shared SD-card root and copy only the latest Commodore 64 package there, leaving emulator project roots and build archives unchanged.

### Fixed

- Applied the MSX-tested USB receive fix so brief SE0/SE1 transitions no longer ended joystick descriptor packets early and prevented detection.
- Preserved receive-edge timestamps and ACK timing while retaining CRC validation, saved calibration, per-port enable settings and C64 port swapping.
- Added receive-edge traces for the first two failed control transactions and timing/packet regressions for both connectors.

## 1.01 - Unreleased

### Added

- Copied the latest prebuilt Commodore 64 firmware package to this emulator's project root and the shared SD-card root when running `make` or `make firmware` from the parent software folder, without rebuilding firmware.

### Fixed

- Restored low-speed USB joystick compatibility by starting raw calibrated reports without requiring a short, recognized HID report descriptor.
- Requested HID idle mode on the selected interface as CP400 did, while allowing controllers that rejected this optional request to continue sending input.
- Preserved calibration, per-port enable settings and C64 port swapping while retaining neutral input for uncalibrated controllers.

## 1.00 - Unreleased

### Added

- Added a standalone PAL Commodore 64 firmware for the shared ESP32-S3 N16R8 VGA board using the retroelec CPU, CIA, VIC-II and SID core.
- Added loading of user-supplied BASIC, KERNAL and character ROMs from the SD card without distributing ROM data.
- Added a USB C64 keyboard matrix with simultaneous keys, cursor and function-key chords, Shift Lock, RUN/STOP and RESTORE.
- Added two calibrated USB joystick inputs with independent enable settings, simultaneous keyboard input and selectable C64 port swapping.
- Added mono PWM sound on GPIO 47 with volume control and muted, flushed output while the F12 menu was open.
- Added PRG loading and read-only program selection from validated 35-track D64 images.
- Added the F12 configuration, media, joystick, reset, SD retry, hardware help and firmware update menus.
- Added independent saved boot settings and calibration, ROM importing, native regression tests and a user-ROM BASIC boot and program-execution smoke test.
- Added `make firmware` to build and verify the shared-format `ESP32_C64-1.00.FLH` package and support switching to and from the other compatible firmwares.

### Notes

- The initial port did not emulate a 1541 drive, IEC disk commands, disk writes, multi-load disk software, tapes, cartridges or snapshots.
- Physical-board performance, VGA, audio, USB and flash/reboot switching remained subject to on-board validation.

### Fixed

- Corrected indirect-Y zero-page pointer wrapping and decimal arithmetic during interrupts for NMOS 6510 software.
