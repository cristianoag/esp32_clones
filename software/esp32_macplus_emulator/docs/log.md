# Firmware Change Log

This log tracks user-visible changes to the ESP32 Macintosh Plus emulator firmware.
Versions use one major digit and two minor digits, for example 1.00 and 1.01.

## 1.03 - Unreleased

### Added

- Added one read-only internal floppy drive for raw 400 KB and 800 KB .dsk/.img files, with F12 browsing, live insertion and ejection after SCSI boot, and Macintosh-requested ejection.
- Added Macintosh GCR sector reads and drive status, stepping, side selection, motor and write-protection behavior while preserving the unmodified ROM.
- Added a separate F activity icon beside SCSI 6, preserving the monitor-safe margins and frame-skip-independent updates.
- Added optional native verification of a runtime-inserted floppy, sector checks across all tracks and sides, and automatic creation of the floppy SD-card directory.

### Notes

- Kept SCSI as the supported boot path and retained existing saved boot settings; floppy assignments were runtime-only and cleared on cold boot.
- Limited floppy support to already formatted, uncompressed raw images; floppy writing, formatting, Disk Copy/NDIF images and 1.44 MB media were not supported.

## 1.02 - Unreleased

### Added

- Added a blinking green disk icon marked 6 in the lower-right VGA border for SCSI reads and writes, with a 200 ms hold and updates independent of frame skipping, matching the CP400 and Apple II indicators without covering the Macintosh display.
- Added cold-boot UART details for the attached SCSI 6 path, sector count and access mode.
- Added an optional native ROM/disk boot test that checked sustained Finder execution without modifying the supplied disk image.

### Fixed

- Corrected VGA horizontal porch and line-length timing so the full picture and F12 menus were no longer shifted right, and moved the disk activity icon farther inside the lower-right border to avoid monitor-edge clipping.
- Fixed the Plus ROM's model detection so a valid SCSI 6 disk was actually accessed instead of being ignored while the central missing-boot-media icon appeared and the corner activity indicator stayed dark.
- Fixed the gray-then-black startup screen by preserving VIA output values written before the ROM configured the port directions, so later disk-control updates no longer selected an unused screen buffer.
- Restricted VIA accesses to the upper byte of the Macintosh data bus so word writes no longer overwrote control registers with their lower byte and unused-byte reads no longer cleared interrupts.

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
