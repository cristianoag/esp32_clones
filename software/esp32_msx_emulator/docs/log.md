# Firmware Change Log

This log tracks user-visible changes to the ESP32 MSX / fMSX firmware.
Versions use one major digit and two minor digits, for example 1.00 and 1.01.

## 1.01 - Unreleased

### Added

- Embedded PicoVerse's 3115-entry SHA-1 mapper database, translated its supported mapper IDs for fMSX, and reported the selected banked mapper on UART.
- Added disk images for drives A and B and read-only CAS tape attachment through F12, with ejection and tape rewind.
- Applied successful disk and tape changes to the running machine without a cold reset and included their paths in saved boot defaults while preserving older settings.
- Added optional user-supplied disk BIOS import for all profiles, using a separate disk slot that preserved Omega and Panasonic startup and Kanji ROMs.

### Changed

- Enabled saving directly to attached DSK files on microSD from Disk BASIC and supported controller writes, synchronizing each sector before reporting success.
- Reported disk write failures to the emulated software and UART, required reattachment after a storage write fault, and rejected mounting the same writable image in both drives.
- Grouped cartridge slots, disks and tape under F12's Media menu with separate ROMs, Disks and Tapes pages.
- Added ROM-page options to reboot with or without saving the complete boot configuration, and prevented rebooting when validation or saving failed.
- Allowed F12 to resume directly from Media pages without rebooting for disk and tape changes, while cartridge changes remained pending until a cold boot.
- Made the MSX firmware independently buildable by including local copies of its board definition, VGA, graphics, BusIO and USB transport dependencies instead of reading files from the CP400 firmware.
- Made the joystick timing and packet tests use the local USB transport headers so the tests also ran without the CP400 project.

### Fixed

- Corrected Tiny Magic 1.1's automatic mapper selection from plain Konami to Konami SCC using its exact ROM fingerprint, while preserving explicit overrides and the fallback for unknown images.
- Reported known unsupported mapper hardware instead of guessing an incompatible mapper.

## 1.00 - 2026-09-10

### Added

- Added Panasonic FS-A1WSX, FS-A1F and FS-A1FX BIOS profiles with 64 KiB RAM defaults and local import of BIOS, sub-ROM, Kanji BASIC and Kanji font components into one ROM file per profile.

- Added independent low-speed USB gamepad input on the two joystick connectors, mapped directions and two fire buttons to MSX joystick ports 1 and 2, and released input when a pad disconnected.
- Added an F12 joystick calibration wizard and live input display, saved each port's calibration by USB device identity, and kept uncalibrated or incompatible reports neutral.

- Added an independent, non-commercial fMSX-based MSX1/MSX2/MSX2+ firmware for the shared ESP32-S3 N16R8 board.
- Matched the existing board's VGA, native USB keyboard, one-bit SD/MMC, and GPIO47 mono audio wiring, and reserved GPIO48 for the onboard LED.
- Added an F12 configuration menu that paused and resumed emulation, selected BIOS profiles and RAM sizes, controlled sound and automatic boot, saved boot defaults, and cold-booted another selected ROM.
- Added active-low MSX keyboard input from a directly connected USB boot keyboard, reserved F12 for configuration, rejected rollover reports, and released held keys on disconnect.
- Added initial Omega MSX2+ NTSC, Gradiente Expert 1.1, and Sharp Hotbit 1.2 BIOS profiles with SD-card ROM loading and explicit errors for missing or malformed files.
- Added a local ROM importer that selected either 256 KiB Omega bank, defaulted to the first bank, preserved the selected bank as one complete ROM, and generated copy-verification checksums without embedding ROMs in firmware.
- Added custom MSX1, MSX2, and MSX2+ boot profiles discoverable from SD without rebuilding firmware.
- Added independent PlatformIO and make build targets and ROM-import regression checks.
- Added CP400-compatible FLH packaging in the dist folder through `make` and `make firmware`, with checksum and payload verification, and removed the selected package through `make clean`.
- Added SD-card cartridge ROM selection and ejection for emulated slots 1 and 2, applied the selected cartridges on cold boot, and saved both assignments with the boot defaults while preserving older settings.
- Added a paginated folder browser for cartridge ROMs and firmware packages, with case-insensitive extension matching and explicit errors for invalid paths or files.
- Added an F12 firmware-update flow with confirmation, progress, FLH validation, inactive-partition installation, and automatic reboot after success.

### Changed

- Replaced the startup screen's USB keyboard pin text with a progress gauge and named initialization stages, retaining the F12 prompt and visible startup warnings.
- Hid the Windows-created System Volume Information folder from cartridge and firmware microSD browsers without deleting it or affecting pagination.
- Redrew the joystick diagnostic screen only when displayed values or menu selection changed, instead of repeatedly flushing an unchanged VGA frame.
- Reduced joystick calibration to neutral, four cardinal directions and two fire buttons, deriving diagonals without asking the user to capture them.
- Preferred internal SRAM for small CPU RAM, BIOS and cartridge allocations while reserving memory for board drivers, and retained PSRAM for large allocations.
- Adapted the percentage of rendered frames to available processing time while keeping the normal emulated Z80 clock, input polling, and sound cadence.
- Reported emulated and displayed frame rates on UART to make real-board speed measurable.
- Paused the software joystick host while saving settings or installing firmware, then restarted device detection before accepting fresh input.

- Placed F12 options on consecutive text rows without blank lines between them.

### Fixed

- Reduced the slow-start rendering ramp by reacting to sustained overload every five PAL or six NTSC frames instead of waiting for repeated half-second adjustments.
- Retained the learned rendering level when resuming from F12 while resetting elapsed-time debt, avoiding another speed ramp after menu use.
- Corrected the original MSX2+ startup animation's beam-timed sprite collisions and SCREEN 6 coarse/fine scrolling, page wrapping and edge masking so the logo moved progressively instead of appearing abruptly.
- Corrected SCREEN 6 sprite color-pair sampling, removing a spurious colored bar from the startup logo.

- Kept Omega's BIOS, BASIC, extension and startup code in one 256 KiB ROM file and mapped the previously omitted Kanji BASIC region containing the original MSX logo, without using either cartridge slot.
- Emulated the MSX2+ reset-status register and visible-scanline sprite-collision polling so the original startup animation could finish and proceed to BASIC.
- Made F12 mute and Sound Off fade GPIO47 to a steady low output and stop its sample timer, rather than continuing a 50% PWM carrier.
- Faded sustained silent audio to a non-switching GPIO47 output, stopped its sample timer when drained, and restarted playback only for new sound.
- Held the last audio sample on buffer underrun instead of abruptly switching to midscale, and ramped playback transitions to reduce clicks.
- Started USB reply decoding at the first K transition so idle-J gaps no longer corrupted ACK synchronization and prevented joystick enumeration.
- Acknowledged validated USB DATA retransmissions promptly, preventing gamepads from repeatedly returning the initial neutral report instead of new control values.
- Allowed joystick reports to settle before capture, learned noise only at neutral, and displayed and logged calibration failures or successful saves explicitly.
- Kept USB receive sampling in internal instruction RAM, bounded its edge buffer, and used CPU-cycle inactivity deadlines instead of compiler-dependent loop counts.
- Allowed at least 10 ms of joystick recovery after USB reset and logged failed descriptor exchanges with packet results and captured-edge counts.
- Paced every emulated frame independently of display updates and avoided an unconditional RTOS sleep on every drawn frame when already running behind.
- Replaced USB NOP-delay calibration with absolute CPU-cycle transmission timing in internal instruction RAM, removing the calibration failure that left the joystick host unavailable.
- Released the USB lines after the end-of-packet J bit instead of holding additional idle bits while a joystick could already be replying.
- Added USB enumeration progress diagnostics and stopped logging missing calibration records as errors on first use.

- Eliminated repeated Library Manager manifest parsing errors when building with the shared VGA library.

### Know issues with this version

- Audio quality is not good, especially at higher sample rates.
- The emulator is a bit slow when compared to the real Z-80
- The firmware does not yet support all MSX hardware features, such as disk drives.

### Notes

- Limited the Panasonic profiles to BIOS/BASIC, startup, cartridge and Kanji support; built-in applications, disk hardware and Panasonic turbo/firmware-mapper features were not included.

- Required user-supplied BIOS images and retained the original fMSX author's non-commercial terms and attribution.
- Used fMSX's generic machine models rather than emulating the physical Omega board's optional flash, disk, and expansion hardware.
- Required directly connected low-speed HID gamepads with learnable reports of at most eight bytes; full-speed controllers and USB hubs were not supported on the joystick ports.
- Left disk and tape browsing and save states outside the initial release.
