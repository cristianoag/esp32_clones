# Reference code, licenses and local changes

This is a provenance report, not a claim that third-party code has been
relicensed or that distribution permissions have been negotiated. Original
notices are retained. The repository's noncommercial terms do not replace
third-party licenses. Compatibility/permissions for distributing the
combined firmware remain to be resolved separately.

## Emulator implementation

| Component | Source | Reported terms |
| --- | --- | --- |
| Z80/R800, V9958/commands, slots, RAM/Panasonic mappers, S1990, Kanji, RTC, TC8566AF, mixer/PSG/DAC/SCC | [libretro/blueMSX-libretro](https://github.com/libretro/blueMSX-libretro), commit `e3086eb5d36d77fa11704cf53dc176686e70127d` | Selected source headers generally specify GPL version 2 or later; some helper/rendering headers use a permissive three-condition zlib-style notice. Check each retained notice. |
| Built-in YM2413 synthesis | blueMSX's `YM2413.cpp` and `OpenMsxYM2413_2.cpp/.h` | The latter explicitly identifies its origin as openMSX, by Mitsutaka Okazaki. The upstream license report classifies the openMSX-derived sound code as GPL. |
| A1ST hardware description and behavior reference | [openMSX](https://github.com/openMSX/openMSX/tree/5ae314755fd1d2e89b0504caad47d2772b043a32), commit `5ae314755fd1d2e89b0504caad47d2772b043a32` | Current openMSX build metadata declares GPL-2.0-only. Its machine definition, Panasonic memory mapping, S1990, E6 timer and PCM behavior were consulted. The modern desktop core is not linked. |
| BIOS/firmware and Kanji ROM | User's Panasonic FS-A1ST dumps | Proprietary third-party ROMs. Not distributed or embedded. Importing locally does not transfer redistribution rights. |

The blueMSX checkout's
[original license report](../lib/bluemsx/upstream-license.txt) is preserved
alongside [GPL version 2](../lib/bluemsx/COPYING). It explicitly states that
blueMSX no longer contains the original noncommercial fMSX source.
Only selected implementation files and their supporting headers are
vendored; no ROM directory, libretro frontend or desktop application was
imported.

## Board and frontend

| Component | Version/origin | Terms present in this checkout |
| --- | --- | --- |
| VGA | Existing board-adapted bitluni ESP32 S3 VGA 0.1.0 | No explicit upstream license file was present in the local driver copy. Attribution and metadata were preserved; permissions must be confirmed rather than assuming MIT/GPL. |
| Adafruit GFX | Existing local 1.12.1 copy | BSD two-clause-style notice in `license.txt`, plus retained notices for bundled material. |
| Adafruit BusIO | Existing local 1.17.1 copy | MIT, in `LICENSE`. |
| USB keyboard adapter | Existing Clone Series MSX adapter | Headers retain The Retro Hacker's personal/noncommercial terms and CP400 attribution. |
| PWM audio, SD browser/path helpers, F12/FLH updater, board definition | Existing Clone Series firmware, copied independently | Repository/source-specific terms retained. These files do not bring in the fMSX emulator. |
| Arduino-ESP32 / ESP-IDF / compiler runtime | PlatformIO `espressif32@6.11.0`, Arduino-ESP32 2.0.17 | Framework and bundled-library licenses apply individually; Arduino core LGPL-2.1 terms and ESP-IDF Apache-2.0 components are relevant. Compiler-runtime exceptions remain applicable to their respective components. |

Existing firmwares and their licenses were not changed. No original
copyright notice was removed. No negotiations or author permissions are
represented as completed.

## Embedded changes to upstream implementation files

The unmodified source baseline is the blueMSX commit above. Changes in this
firmware's independent copy:

- `romMapperDRAM.c`: bounded each mapping by its real ROM size; preserved
  the upper Kanji BASIC pages outside the DRAM overlay.
- `romMapperPanasonic.c`: permitted SRAM/RAM writes through every mapped
  region and exposed BIOS/sub-ROM DRAM overlays through the firmware
  mapper, following openMSX's hardware behavior.
- `romMapperTurboRTimer.c`: used the exact master-clock/84 E6 frequency,
  preserved oscillator phase on reset, and ignored E7 writes.
- `VDP.c`: reset the cached scanline offset at a new frame, avoiding a
  stale last-row pointer overflowing the compact single framebuffer.
- `RTC.c/.h`: avoided an ESP-IDF header-name/guard collision and exposed
  checked binary CMOS writes and synchronization.
- `OpenMsxYM2413_2.cpp/.h`: placed the two large shared lookup tables in
  dynamically allocated PSRAM instead of overflowing internal BSS; scaled
  the 44100 Hz modulation constants for the board's 22050 Hz output.
- `SCC.c`: imported from the same pinned blueMSX commit with its original
  notices; adjusted the phase step to the active mixer sample rate.

The `port` directory implements the embedded scheduler, A1ST construction,
framebuffer, raw sector I/O, persistence, host-service boundary and PCM
integration. It also provides independent cartridge mapping for the eight
normal MSX mapper choices, using blueMSX SCC and the existing YM2413
integration without linking the noncommercial fMSX core. Desktop save-state calls fail explicitly if accidentally
invoked; that feature is not advertised by the frontend.
