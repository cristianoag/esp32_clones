# Firmware Change Log

This log tracks user-visible changes to the ESP32 MSX TurboR / FS-A1ST firmware.
Versions use one major digit and two minor digits, for example 1.00 and 1.01.

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
