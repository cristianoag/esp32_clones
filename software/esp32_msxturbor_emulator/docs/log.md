# Firmware Change Log

This log tracks user-visible changes to the ESP32 MSX TurboR / FS-A1ST firmware.
Versions use one major digit and two minor digits, for example 1.00 and 1.01.

## 1.04 - Unreleased

### Changed

- Changed the default render/presentation interval to six frames and migrated saved records still using the previous two-frame default, while retaining other intervals and allowing two to be saved explicitly afterward.

### Fixed

- Buffered UART transmission so periodic multi-line diagnostics did not block emulation while bytes were sent, and staged PSRAM PCM reads outside the audio interrupt lock.
- Amplified the quiet mixer signal before rounding it to 8-bit PWM, with saturating output and a default 8x gain, so low-level game music no longer collapsed to only a few duty levels.
- Excluded intentional audio-timer sleep from late-interrupt gap diagnostics.
- Corrected PSG tone, noise and envelope clocks for 22050 Hz synthesis; MSX1 music previously played at half its intended pitch.
- Restored fixed 22050 Hz PWM playback, matching the ordinary MSX firmware instead of lowering music pitch when emulation ran slowly, while retaining buffering and explicit underrun reporting.
- Preferred internal SRAM for CPU state with a 64 KiB reserve, and skipped inactive FM/silent DAC work to reduce unnecessary emulation overhead.
- Removed per-pixel VGA divisions and replaced repeated PSG phase loops with mathematically equivalent averaging to reduce frame cost and native-rate audio starvation without changing pixels or sound samples.
- Skipped VDP pixel composition on undisplayed frames while retaining every guest frame, sprite collision/overflow evaluation, VBlank and VDP command progress.
- Grouped VGA cache writeback into aligned eight-row spans, reducing cache calls from 240 to 30 per presentation without changing output pixels.

### Added

- Added sampled CPU/VDP/audio work-time and framebuffer-copy reports with a unique build marker to identify the remaining real-time bottleneck on the board.
- Added saved 1x/2x/4x/8x PWM gain selection and a separate output-clipping diagnostic; the reference tone always used 1x gain.
- Added saved F12 switches for PSG, MSX-MUSIC, TurboR PCM, SCC and keyboard-click output, retaining chip/register behavior and migrating older settings with all sources enabled.
- Added a PSG-only diagnostic preset, a source-independent PWM test tone and optional five-second UART reports of signal levels, clipping, ISR timing and sound-chip state to help isolate persistent board-reported noise.
- Added audio playback rate, queued-sample and underrun figures to the regular UART performance diagnostic.

## 1.03 - Unreleased

### Changed

- Changed the parent-folder `make` and `make firmware` commands to clear FLH files from the shared SD-card root and copy only the latest MSX TurboR package there, leaving emulator project roots and build archives unchanged.

### Fixed

- Stopped rereading the entire SD directory on every file-browser key press, keeping movement within the displayed page responsive while refreshing entries on page/folder changes and reopening.
- Allowed valid cartridge ROMs up to 4 MiB and added automatic/manual ASCII16-X banking for images such as Illusion City, which previously failed the 2 MiB size check.
- Loaded cartridge ROMs in separate PSRAM banks so large images did not require a contiguous 4 MiB allocation after machine startup.
- Removed full-ROM temporary allocations when selecting cartridges in F12 and shared identical read-only firmware banks, reclaiming about 0.75 MiB with the tested ST dump so a 4 MiB cartridge could fit in PSRAM alongside the machine.
- Added requested/allocated ROM sizes and free/largest PSRAM figures to cartridge allocation errors while retaining the previous cartridge on failure.
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
