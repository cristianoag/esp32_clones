# Firmware Change Log

This log tracks user-visible changes to the ESP32 Apple II emulator firmware.
Versions use one major digit and two minor digits, for example 1.00 and 1.01.

## 1.03 - Unreleased

### Changed

- Allowed Firmware update to install compatible CP400, MSX and TK95 FLH packages as well as Apple II packages without renaming, while retaining image, checksum and OTA layout validation.
- Extended firmware-update host tests to install all four emulator packages into mock OTA storage and compare the written payloads.
- Reduced CPU memory-access overhead with precomputed RAM/ROM page mappings and an inlined cycle loop, while retaining the original CPU clock and bank-switch behavior.
- Sent rendered scanlines directly to the VGA framebuffer, removing the intermediate 150 KiB PSRAM framebuffer and its extra copy without reducing resolution.
- Stopped repainting text screens when DOS loaded unrelated graphics memory, while still refreshing modified visible pages and auxiliary display memory.

### Added

- Added checks for all RAM-bank flag combinations, language-card aliases and equivalence between individual CPU ticks and complete frame execution.

### Notes

- On-board measurements of the previous firmware showed about 20.6 ms of CPU work per emulated frame, already above the 16.6 ms budget for 60 fps; the new optimizations still required fresh hardware measurements.

## 1.02 - Unreleased

### Changed

- Moved the 6502 decoder and memory-access hot path into internal instruction RAM to reduce flash-cache overhead during emulation.
- Rendered display rows in internal RAM before copying them to PSRAM and stopped redrawing unchanged screens, while preserving 80-column output, flashing text and display-page changes.
- Avoided calculating unused floating-bus values during keyboard, status and Disk II data-latch reads.

### Added

- Added CPU, video, framebuffer-copy and audio timing to UART diagnostics, alongside redraw counts, the CPU program counter, drive-1 track and elapsed emulated time.
- Added a 512-configuration framebuffer-equivalence regression and checks for display invalidation, including auxiliary memory and flashing text.

### Notes

- Full-speed operation still required new on-board measurements; the previous firmware ran at roughly 20 fps with every-frame rendering and 30.6 fps with every-third-frame rendering on the reported board.

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
