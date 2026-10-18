# ESP32 MSX TurboR / Panasonic FS-A1ST

Experimental, standalone A1ST firmware for the shared **ESP32-S3 N16R8**
Clone Series board. It uses a source subset of **blueMSX**, with openMSX as
the machine-layout and device-behavior reference. It does not use the
noncommercial fMSX core from the separate MSX firmware.

The original user-supplied A1ST BIOS boots on the host integration into
**MSX BASIC**, including actual Z80/R800 switching and DRAM mode.
**Physical-board performance, VGA/audio quality and firmware switching
still require validation.** A successful build is not a full-speed claim.

## Hardware and dependencies

The board, PlatformIO version and local VGA/GFX/BusIO drivers match the
other Clone Series firmwares. No sibling firmware directory is needed to
build this folder. The original dependency notices have been retained;
see [licenses and provenance](docs/licenses.md).

| Function | GPIO |
| --- | --- |
| VGA red low/high | 5 / 4 |
| VGA green low/high | 7 / 6 |
| VGA blue low/high | 9 / 8 |
| VGA HSYNC / VSYNC | 2 / 1 |
| Native USB keyboard D- / D+ | 19 / 20 |
| SD/MMC CMD / CLK / D0 | 38 / 39 / 40 |
| Mono PWM audio | 47 |
| Onboard RGB LED | 48 |

Use the UART port for upload/diagnostics. Native USB is the keyboard host,
not a serial console. An 8 MiB OPI PSRAM module is required.

## Prepare the SD card

Use a FAT32 card. From this firmware folder:

```powershell
.\tools\Import-Roms.ps1 `
    -SourceDirectory 'C:\Program Files\openMSX\share\systemroms\systemroms\machines\panasonic' `
    -Destination .\sdcard
```

Copy the generated `sdcard\msxturbor` directory to the card root, alongside
the other emulators' directories. Alternatively, set `-Destination` to the
SD drive root. The importer validates complete file sizes and the hashes
accepted by openMSX before writing anything. Different existing ROMs are
not overwritten without `-Force`. It never modifies the originals.

Required files on the card:

```text
msxturbor/bios/fs-a1st/fs-a1st_firmware.rom   4194304 bytes
msxturbor/bios/fs-a1st/fs-a1st_kanjifont.rom   262144 bytes
msxturbor/disks/                            optional .dsk images
```

The importer accepts both known ST firmware revisions; select the alternate
with `-FirmwareRom 'C:\path\to\fs-a1st_firmware_other_version.rom'`.
Keep the full 4 MiB dump. The emulated ST firmware mapper exposes its first
256 8 KiB banks, with mirroring as in the openMSX ST definition.
BIOS, opening, disk, Kanji BASIC and MSX-MUSIC regions are mapped from that
image; no additional disk BIOS or extracted sidecar ROMs are needed.
The GT firmware is intentionally rejected.
ROM loading stages 4 KiB at a time through aligned internal stack memory
before copying into PSRAM, yielding between reads to service board tasks.

No BIOS or software images are distributed or embedded in the application.

## Build and upload

```powershell
pio run
make firmware
make upload
```

`make firmware` builds and verifies `dist\ESP32_MSXTURBOR-1.00.FLH`.
`make upload` is a UART upload, including the shared partition table.
The Makefile uses PlatformIO's standard Windows virtual environment.

## F12 configuration

F12 pauses the emulated machine and sound, backs up the VGA picture, and
opens the same 320x240-style configuration interface used by the other
firmwares. Use arrows and Enter; Esc or F12 resumes a booted machine.

- **Drive A:** browse raw 360/720 KiB `.dsk` files; Delete ejects. Changes
  are live while paused. A failed attachment preserves the old image.
- **Sound:** PSG, built-in MSX-MUSIC and TurboR PCM output.
- **Built-in firmware:** controls the original firmware switch; cold-boot
  to apply it. Off is the default for normal BASIC/DOS startup.
- **Auto boot** and **Present every N frames:** independent host settings.
  Presentation skipping does not accelerate the guest CPU or skip VDP
  state/sprite evaluation.
- **Save boot configuration:** saves settings and the disk path in its own
  NVS namespace, separate from the ordinary MSX emulator.
- **Cold boot:** discards the emulated machine state and applies pending
  firmware-switch settings.
- **Rescan SD:** only remounts while no machine is running.
- **Firmware update / switch emulator:** installs a compatible `.FLH`.

Disk writes are synchronous and persistent in the original DSK, not a
temporary overlay. A write/sync error is reported and further writes are
blocked until remount. Back up images; power loss can leave partial writes.
Keep the SD card inserted while the machine is running. Formatting and
nonstandard/protected floppy formats are not validated; use existing raw
360/720 KiB images.

Entering F12 flushes firmware SRAM and CMOS. These are separate
`fs-a1st.sram` and `fs-a1st.cmos` files in the BIOS directory. Use F12 before
powering off after changing persistent emulated settings.

The keyboard uses the original Japanese machine's physical matrix.
Punctuation follows that ROM's JIS layout, not necessarily the printed
legends on a US/Brazilian keyboard.

## Switching between firmwares

With the **current updater and shared dual 4 MiB OTA layout** installed,
any existing Clone Series firmware can install this FLH without renaming
it. This firmware can install CP400, ordinary MSX, TK95, Apple II or another
TurboR FLH through F12. Put packages in the SD root for compatibility with
all file pickers, prepare destination ROMs, confirm, and keep power on.

The updater is the same validated, two-pass SD/OTA implementation as the
MSX firmware: it checks package bounds, checksum, ESP32-S3 image, second-pass
consistency and ESP-IDF image verification before selecting the inactive
slot. The package remains on SD. The checksum is not an authenticity
signature.

Old filename-restricted updaters must first install a current build of the
same emulator. Old partition layouts require a one-time UART upload;
an FLH cannot replace the partition table. Use current destination builds
so switching remains available afterward.

The repository-level `software\Test-FirmwareCompatibility.ps1` includes
this firmware and checks every source/destination combination in its
configured project list. Build all listed packages before running it.

## Implementation scope and limitations

Included: 256 KiB mapped RAM, 16 KiB firmware SRAM, 128 KiB V9958 VRAM,
separate Z80/R800 register banks, S1990 CPU/DRAM switching, Panasonic ROM
banking and DRAM aliases, Kanji ROM, PPI keyboard, PSG, MSX-MUSIC, RTC,
non-inverted reset status, E6/E7 timer, PCM output, pause/LED registers and
the original banked TC8566AF disk interface.

Not claimed: A1GT/MIDI, cartridge or cassette selection, USB joystick
support, microphone recording, printer output, save states, reverse
execution, cycle-perfect R800/VDP timing or complete application
compatibility. The upstream CPU timing model is retained; openMSX is a
reference, not the linked desktop CPU core. PCM input reads a neutral
signal because this board has no microphone path. Mechanical disk noise
is omitted.

VGA reuses the existing 320x240, line-doubled driver. High-resolution and
80-column output are downsampled; interlaced fields are not woven into a
480-line image. UART reports **emulated speed percentage**, presented FPS,
active CPU/DRAM mode and free PSRAM every five active seconds. Menu time
is excluded. Measure these on the board before drawing performance
conclusions.

## Validation

```powershell
make test
.\tools\Test-Core.ps1 `
    -Firmware 'C:\path\to\fs-a1st_firmware.rom' `
    -Kanji 'C:\path\to\fs-a1st_kanjifont.rom'
.\tools\Test-RomImport.ps1 -PanasonicDirectory 'C:\path\to\panasonic'
```

Host tests use native GCC/G++. They cover ROM/DRAM/SRAM mapping, S1990,
R800 multiplication, E6 timer phase, keyboard matrix, original TC8566AF
sector reads/writes, persistence, framebuffer bounds, VGA geometry,
settings validation, ROM importing and the shared FLH updater.
ROM-backed boot tests read your originals in place and keep generated
state only under the ignored `tests\build` directory.
