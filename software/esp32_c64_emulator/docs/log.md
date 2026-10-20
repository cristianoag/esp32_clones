# Firmware Change Log

This log tracks user-visible changes to the Commodore 64 firmware.
Versions use one major digit and two minor digits, for example 1.00 and 1.01.

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
