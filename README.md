# The Retro Hacker Clone Series

The Retro Hacker Clone Series brings together hardware designs and source-available firmware to recreate and emulate classic Brazilian computers from the 80s using modern ESP32-S3 based hardware.

This repository is the host project for the series. It brings together the shared hardware work, firmware projects, local KiCad libraries, documentation, and supporting assets used to build ESP32-based recreations of multiple machines from Brazil's 8-bit home computer era.

The repository currently contains two firmware projects for the shared board:

| Firmware | Emulated machines | Project |
| --- | --- | --- |
| CP400 | Prologica CP400 / CoCo 2 | [Firmware](software/esp32_cp400_emulator/) and [change log](software/esp32_cp400_emulator/docs/log.md) |
| MSX | MSX1, MSX2, and MSX2+ | [Setup guide](software/esp32_msx_emulator/README.md) and [change log](software/esp32_msx_emulator/docs/log.md) |

CP400 builds on the earlier ESP32 CP400 project, while MSX integrates Marat Fayzullin's original fMSX core. Both share the same practical goal: usable, hackable retrocomputers with VGA output, USB keyboard input, SD-backed storage, and on-screen F12 configuration. 

## Project Vision

The goal is not to build museum props. The goal is to create practical, buildable modern recreations that preserve the character, software culture, and hardware feel of Brazilian 1980s computers while making them approachable for today's builders.

Each clone in the series is expected to combine two sides of the work:

- Hardware designs that give each emulator a dedicated physical form.
- Firmware that recreates the original computer environment on ESP32-based hardware.

The project is designed for retrocomputing enthusiasts, hardware hackers, firmware developers, and anyone interested in studying and preserving Brazil's microcomputer history through working machines.

## Prologica CP400

The Prologica CP400 was one of the memorable Brazilian home computers of the mid-1980s. Built during Brazil's market-reserve era, it was compatible with the Tandy/Radio Shack TRS-80 Color Computer 2, known as the CoCo 2, but arrived with its own Brazilian industrial design, PAL-M video expectations, local peripherals, translated software, and a particular place in the local 8-bit scene.

Under the hood, the original CP400 family followed the CoCo architecture closely:

- Motorola MC6809E CPU running around 0.895 MHz
- Motorola MC6847 video display generator
- 16 KB or 64 KB RAM configurations
- Extended Color BASIC in ROM
- Cassette, cartridge, joystick, serial, video, and expansion interfaces
- Optional disk support through the CP450 floppy system

The [CP400 firmware](software/esp32_cp400_emulator/) implements the CoCo 2 / CP400 environment on ESP32-S3 hardware, including MC6809 emulation, video output, native USB keyboard input, joystick input paths, SD storage, virtual floppy disks, and an F12 menu with firmware-update support.

## MSX1, MSX2, and MSX2+

The [MSX firmware](software/esp32_msx_emulator/README.md) uses the original fMSX core with an independent integration for the shared board. It provides SD-loaded BIOS profiles for Omega MSX2+, Gradiente Expert 1.1, and Sharp Hotbit 1.2, plus an F12 configuration and boot-ROM selection menu.

MSX1, MSX2, and MSX2+ share one firmware project and the existing VGA, USB keyboard, SD/MMC, and mono audio wiring. Bring your own legally obtained BIOS files; they are not embedded in the firmware or distributed by this repository.

The MSX firmware provides BIOS/BASIC and cartridge boot, keyboard input, graphics, sound, and F12 configuration. The menu selects BIOS profiles, SD-card ROMs for two cartridge slots, RAM size, sound, and saved boot defaults, and installs MSX FLH firmware updates from SD. Additional BIOS profiles and cartridge ROM files can be added without rebuilding firmware.

All three initial BIOS profiles reached BASIC's `Ok` prompt in host emulation tests, and the ESP32-S3 firmware builds successfully. Physical-board VGA, USB, audio, cartridge compatibility, firmware updates, and real-time performance validation remains outstanding. Disk/tape browsers, joystick support and save states are not yet exposed by the MSX firmware; the Omega profile uses fMSX's generic MSX2+ model rather than emulating all physical Omega expansions.

## Repository Layout

```text
hardware/esp32_clones/          KiCad project for the shared ESP32 clone hardware
hardware/esp32_clones/libraries Project-local KiCad symbols and footprints
software/esp32_cp400_emulator/  PlatformIO firmware for the CP400 emulator
software/esp32_msx_emulator/    MSX1/MSX2/MSX2+ firmware and F12 boot profiles
images/                        Project images and documentation assets
```

## Hardware

The KiCad project lives in `hardware/esp32_clones` and is named `esp32_clones`.

The local KiCad libraries are organized inside:

```text
hardware/esp32_clones/libraries/symbols
hardware/esp32_clones/libraries/footprints
```

The hardware design includes project-local footprints and symbols for the ESP32-S3 module, USB connectors, SD card socket, switches, and other board-level parts. The intent is to give the firmware a board that feels more like a small computer than a loose development kit on a bench.

Both firmware projects target an ESP32-S3 DevKitC-style N16R8 module with 16 MB flash and 8 MB OPI PSRAM. They use the same VGA pinout, native USB keyboard port on GPIO19/20, one-bit SD/MMC interface on GPIO38/39/40, and mono audio output on GPIO47. GPIO48 belongs to the onboard RGB LED, not a second audio channel. See the [MSX hardware table](software/esp32_msx_emulator/README.md#hardware) for the full shared pin assignments.

## Firmware

Both projects use PlatformIO with the Arduino framework and a pinned `espressif32@6.11.0` platform. Run build commands from the firmware directory you want to use:

```powershell
# Choose one project:
Set-Location .\software\esp32_cp400_emulator
# Or, from the repository root:
# Set-Location .\software\esp32_msx_emulator

make firmware
```

`make firmware` builds and verifies a versioned FLH application package in that project's `dist` folder:

- CP400: `ESP32_CP400-<version>.FLH`
- MSX: `ESP32_MSX-<version>.FLH`

The FLH container format is shared, but the applications and partition layouts are separate. **Use USB/UART upload when switching between CP400 and MSX; do not use either emulator's F12 updater to install the other.** Once installed with the correct partition layout, MSX can install its own `ESP32_MSX-*.FLH` packages through **F12 > Firmware update from SD**.

With PlatformIO on PATH, `pio run` builds the raw application and `pio run -t upload --upload-port COM18` uploads through the board's PC/UART port. Replace `COM18` with the actual port. The native USB controller is reserved for the keyboard.

Keep both firmware folders in a checkout: the MSX project currently reuses the board definition and VGA/GFX/BusIO libraries from the CP400 directory. Follow the [MSX setup guide](software/esp32_msx_emulator/README.md) to prepare BIOS profiles on a FAT32 SD card before booting it.

## ROMs and Original Software

Original CP400, CoCo, MSX, BASIC, cartridge, cassette, and floppy software may still be copyrighted. This repository is for original hardware, firmware, and project files. Bring your own legally obtained ROMs and software images when needed.

## Historical Notes

The CP400 was part of a wider Brazilian TRS-Color ecosystem that included machines such as the Codimex CD-6809, LZ Color 64, Dynacom MX-1600, and Varix VC50. The MSX line later became one of the most important home computer families in Brazil, with a large software, game, education, and hobbyist culture.

The Retro Hacker Clone Series exists at the meeting point of those histories: Brazilian microcomputer design, international 8-bit architectures, local software preservation, and the practical charm of machines that can be built, modified, and used today.

## References and Further Reading

- [Prologica CP-400](https://en.wikipedia.org/wiki/Prol%C3%B3gica_CP-400)
- [CP400](https://pt.wikipedia.org/wiki/CP400)
- [TRS-80 Color Computer](https://en.wikipedia.org/wiki/TRS-80_Color_Computer)
- [MSX](https://en.wikipedia.org/wiki/MSX)
- [fMSX by Marat Fayzullin](https://fms.komkon.org/fMSX/), the core used by the MSX firmware
- [S3-MSX-PC](https://github.com/Svarkovsky/s3-msx-pc), a reference project for MSX emulation on ESP32-S3; this repository uses an independent integration, not its combined firmware
- [Prologica](https://pt.wikipedia.org/wiki/Prol%C3%B3gica)
- [Datassette](https://datassette.org/) for Brazilian retrocomputing manuals, magazines, books, and software preservation material

## License

See [LICENSE.txt](LICENSE.txt) for the repository's Attribution-NonCommercial-ShareAlike 4.0 license. Firmware and third-party components retain their own terms; source availability does not imply unrestricted commercial use.

In particular, fMSX is non-commercial/source-available software by Marat Fayzullin, not an unrestricted open-source core. Commercial use requires a separate license from its author. See the [fMSX attribution and porting notes](software/esp32_msx_emulator/lib/fmsx/README.md) and each component's notices before reusing or redistributing project materials. Firmware licenses do not grant rights to redistribute original BIOS or game images.
