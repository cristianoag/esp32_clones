# Firmware Change Log

This log tracks user-visible changes to the ESP32 Apple II emulator firmware.
Versions use one major digit and two minor digits, for example 1.00 and 1.01.

## 1.01 - Unreleased

### Fixed

- Kept Disk II motors running for the hardware's one-second stop delay, so DOS no longer repeated its spin-up wait between sector reads and made booting Karateka appear frozen.

### Added

- Added an optional local Karateka disk regression covering cold boot, `PR#6`, the intro and keyboard-driven entry into the game on both supported machines.

## 1.00 - Unreleased

### Added

- Added standalone Apple II+ and original Apple IIe profiles with 64 KiB and 128 KiB of RAM respectively, using user-supplied machine and Disk II ROMs from the `apple2/bios` SD folder.
- Added full-width 80-column text on the shared VGA board, alongside 40-column text, inverse/flashing characters, lo-res, hi-res, mixed and IIe double-resolution graphics.
- Added native USB keyboard input with control codes, Caps Lock, key repeat and Open/Solid Apple buttons.
- Added two calibrated USB joystick ports mapped to Apple paddles and pushbuttons, and mono speaker audio on GPIO47.
- Added two write-protected slot-6 Disk II drives for 35-track NIB images, with mount and eject actions in F12.
- Added the shared F12 workflow for configuration, saved boot defaults, warm reset, cold boot, SD retry, joystick calibration and firmware updates.
- Added `make firmware` packaging and verification for `ESP32_APPLE2-1.00.FLH`, with Apple-specific filenames and the shared dual-slot OTA layout.
- Added ROM import validation, native regression tests and optional ROM smoke tests covering BASIC, 80-column output and a synthetic NIB boot sector.

### Notes

- Brazilian clone profiles and enhanced IIe/65C02 support were not included in this initial release.
- ROMs and games were not distributed; text used the existing ASCII font rather than a bundled Apple character ROM.
- Disk writes, cassette, snapshots and sector-image formats were not supported.
- Host tests and the ESP32-S3 build were verified, but physical-board operation and real-time speed still required hardware validation.
