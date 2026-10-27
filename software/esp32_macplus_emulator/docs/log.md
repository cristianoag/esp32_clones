# Firmware Change Log

This log tracks user-visible changes to the ESP32 Macintosh Plus emulator firmware.
Versions use one major digit and two minor digits, for example 1.00 and 1.01.

## 1.01 - Unreleased

### Changed

- Changed the parent-folder `make` and `make firmware` commands to clear FLH files from the shared SD-card root and copy only the latest Macintosh Plus package there, leaving emulator project roots and build archives unchanged.

### Fixed

- Applied the MSX-tested USB receive fix to the mouse transport so brief SE0/SE1 transitions no longer ended descriptor packets early and prevented detection.
- Preserved receive-edge timestamps and ACK timing while retaining CRC validation and boot-mouse protocol selection on joystick connector 2 only.
- Added receive-edge traces for the first two failed control transactions and timing/packet regressions without enabling joystick connector 1 or changing mouse controls.

## 1.00 - Unreleased

### Added

- Copied the latest prebuilt Macintosh Plus firmware package to this emulator's project root and the shared SD-card root when running `make` or `make firmware` from the parent software folder, without rebuilding firmware.
- Added standalone Macintosh Plus firmware with a Musashi 68000, 4 MiB of RAM and a centered native 512x342 monochrome display on VGA.
- Loaded a checksum-verified, user-supplied 128 KiB Plus ROM and a writable whole SCSI disk image from SD.
- Added USB keyboard input with Command, Option, latched Caps Lock and extended arrow/keypad events.
- Supported a direct low-speed HID boot mouse on joystick port 2, with signed relative movement, the left button and release on disconnect.
- Added mono speaker audio on GPIO47 with sound and volume controls.
- Added the F12 menu with ROM and disk browsing, next-boot disk protection and ejection, automatic boot, frame skipping, saved boot settings, cold boot confirmation and hardware help.
- Preserved Macintosh PRAM separately from other emulators' settings.
- Added firmware updates compatible with the shared dual 4 MiB OTA layout and other emulators' FLH packages, without renaming or deleting source files.
- Added native regression tests, firmware packaging and setup documentation with source references and validation limitations.
- Added a verified local ROM/disk importer and blank SD-image creation, preserving existing disk images and excluding private ROM/IMG assets from Git.

### Notes

- Required user-supplied Macintosh ROM and system disk images; no Apple software was bundled.
- Limited the second connector to low-speed HID boot mice on the existing wiring; full-speed-only mice and hubs required different host hardware.
- Left physical-board boot, mouse, display, audio and firmware-switching validation pending.
