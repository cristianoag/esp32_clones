# ESP32 MSX / fMSX

This is the MSX firmware for the Retro Hacker Clone Series board. It uses the original fMSX core and targets the shared ESP32-S3 N16R8 hardware used by the rest of the project.

## Hardware

The hardware layout matches the shared board used by the CP400 project and is documented here for reference.

| Function | GPIO |
| --- | --- |
| VGA red, low/high bits | 5 / 4 |
| VGA green, low/high bits | 7 / 6 |
| VGA blue, low/high bits | 9 / 8 |
| VGA HSYNC / VSYNC | 2 / 1 |
| USB keyboard D- / D+ | 19 / 20 |
| USB joystick 1 D- / D+ | 15 / 16 |
| USB joystick 2 D- / D+ | 17 / 18 |
| SD/MMC CMD / CLK / D0 | 38 / 39 / 40 |
| Mono PWM audio | 47 |
| Onboard RGB LED | 48 |

SD uses a one-bit SD/MMC interface rather than SPI. Connect a USB HID keyboard directly to the keyboard port, and use the board's UART port for upload and diagnostics. The native USB controller is reserved for the keyboard.

The firmware is self-contained: its board definition lives in `boards`, and
its VGA, Adafruit GFX, BusIO and USB transport sources live in `lib`.
The entire `esp32_msx_emulator` folder can be copied and built on its own;
the CP400 firmware folder is not required. PlatformIO and the pinned ESP32
platform/toolchain are still required.

The local dependency copies preserve their original versions, copyright
notices and licenses. See [dependency provenance](lib/README.md).

## Build

From this firmware directory, run `pio run` to build or `make firmware` to
build, package and verify `dist\ESP32_MSX-1.01.FLH`. The same commands work
when this folder is outside the clone-series repository. `make test` runs
the host regression tests using PowerShell and native G++.

If updating an older checkout that used cross-project symlinks, PlatformIO
normally removes obsolete dependencies during the next build. If local
cache metadata still points to the old project, delete this firmware's
generated `.pio` directory and rebuild; no CP400 files need to be changed.

## Prepare the SD card

Use a FAT32 card. BIOS files are supplied by the user and loaded from SD rather than being built into the firmware. From this firmware directory:

```powershell
.\tools\Import-Roms.ps1 -Destination .\sdcard `
    -OmegaRom 'C:\path\to\omega_msx2+_all_ntsc.bin' `
    -ExpertRom 'C:\path\to\expert_1.1_basic-bios1.rom' `
    -HotbitRom 'C:\path\to\hotbit_1.2_basic-bios1.rom'
```

Copy the generated `sdcard\msx` folder to the card root. Alternatively, use the SD drive root as the destination.

The initial profiles are:

| Profile directory | Machine | Default RAM | BIOS files |
| --- | --- | --- | --- |
| `msx\bios\omega` | MSX2+ | 512 KiB | `OMEGA.ROM` |
| `msx\bios\expert` | Gradiente Expert 1.1 / MSX1 | 64 KiB | `MSX.ROM` |
| `msx\bios\hotbit` | Sharp Hotbit 1.2 / MSX1 | 64 KiB | `MSX.ROM` |
| `msx\bios\fs-a1wsx` | Panasonic FS-A1WSX / MSX2+ | 64 KiB | `PANASONIC.ROM` |
| `msx\bios\fs-a1f` | Panasonic FS-A1F / MSX2 | 64 KiB | `PANASONIC.ROM` |
| `msx\bios\fs-a1fx` | Panasonic FS-A1FX / MSX2+ | 64 KiB | `PANASONIC.ROM` |

## Media menu

Open **F12 > Media** to choose **ROMs**, **Disks**, **Tapes** or **Audio profile**.
The ROMs page contains cartridge slots 1 and 2; the Disks page contains
drives A and B; the Tapes page contains the tape selection and rewind.
Audio can also be opened from each of those pages and applies to the whole machine.
Press Enter to browse for an image or Delete to eject the selected slot,
drive or tape. Esc goes back one menu level. From these menu pages, F12
resumes a running machine directly.

### Cartridge ROMs and rebooting

In **Media > ROMs**, select either or both cartridge slots, then choose:

- **Reboot and save configuration:** save the complete current boot
  configuration (BIOS, RAM, sound, auto-boot, both cartridges, both disks
  and tape, cartridge mapper overrides and the audio profile), then cold-boot with those selections.
- **Reboot without saving:** cold-boot with the selected configuration for
  this session without overwriting the saved defaults.

Both options discard the current emulated machine state. A validation or
save failure leaves the menu open and does not reboot. Cartridge insertion
and ejection take effect only on cold boot. Leaving F12 without choosing
a reboot option resumes with the previously running cartridges; your new
selections remain pending until a cold boot.

### Automatic cartridge mapper detection

After selecting a cartridge, the ROMs page shows its detected mapper and
the detection source. Select the **Mapper** row below either slot and use
Left/Right (or Enter) to cycle between **Auto** and the supported manual
mappers. Delete on a mapper row restores Auto; Delete on a slot row ejects
the cartridge. Replacing or ejecting a ROM resets that slot's override to
Auto; choosing the same file preserves its override.

The detected mapper remains visible even when you choose a manual override.
Inspection uses the selected BIOS profile's overrides without changing a
running machine. Overrides take effect only on cold boot and are saved with
the complete boot configuration. Older saved configurations migrate with
both slots set to Auto. Failed file reads or inspection leave the previous
cartridge and override selected.

Banked cartridges now use the same **full-ROM SHA-1 database-first approach**
as PicoVerse. The 3115-entry database is embedded in the firmware and shared
by all BIOS profiles; no external database or PicoVerse directory is needed.
PicoVerse mapper IDs are translated to fMSX's Konami SCC, Konami, ASCII8 and
ASCII16 IDs. The selected banked mapper is reported on UART.

Manual F12 mapper choices take priority. In Auto mode, existing profile-local
`CARTS.CRC` / `CARTS.SHA` overrides still take priority over the embedded database.
Unknown cartridges retain the existing fMSX heuristic and plain/planar ROM
handling. Identification does not add hardware emulation: recognized
ASCII16-X, Manbow2, NEO8 and NEO16 cartridges report an unsupported mapper
instead of silently pretending to be a supported type.
See [database provenance](lib/fmsx/MapperDatabase/README.md).

**Tiny Magic 1.1:** the verified 512 KiB ROM with SHA-1
`b9664e6094d8488e8e0b8103b1be5443d8da0cd0` now selects Konami SCC automatically,
instead of plain Konami. Host tests also found two independent setup
requirements: at least **128 KiB RAM** and an **MSX-MUSIC/FM BIOS**.
Panasonic profiles default to 64 KiB, so select the profile first, then
change RAM to 128 KiB or more and cold-boot. Omega's 512 KiB is sufficient.
Use the FM-PAC/MSX-MUSIC audio profile described below. The original 16 KiB
`fs-a1wsx_fmbasic.rom` also works as an MSX-MUSIC BIOS. Correct mapper
detection alone cannot resolve missing RAM or the game's
"No FM PAC nor MSX MUSIC detected" message.

### Audio profiles and FM-PAC / MSX-MUSIC

The **Audio profile** setting controls the emulated sound hardware:

- **Auto:** preserves the normal PSG/SCC setup and enables FM when a compatible BIOS is available.
- **PSG only**
- **PSG + SCC**
- **PSG + FM-PAC/MSX-MUSIC**
- **PSG + SCC + FM-PAC/MSX-MUSIC**

This is one machine-wide setting for cartridges, disks and tapes, not a
separate setting for every file. Changing it requires a **cold reboot**.
The Audio page offers reboot with or without saving the complete boot
configuration. Merely resuming F12 leaves the currently running audio
hardware unchanged. The main menu's Sound toggle remains the mute control.

FM profiles need a legally obtained FM BIOS on microSD; no BIOS bytes are
embedded in the firmware. Import a 64 KiB FM-PAC BIOS, such as your
`FMPCCMFC.BIN`, or a compatible 16 KiB MSX-MUSIC BIOS:

```powershell
.\tools\Import-AudioBios.ps1 -Rom 'C:\path\to\FMPCCMFC.BIN' -Destination .\sdcard
```

Copy the generated `msx\audio\FMPAC.ROM` to the same path on the card.
This shared BIOS is available to every machine profile; a profile-local
`msx\bios\<profile>\FMPAC.ROM` takes precedence. The importer verifies size,
identification header and the copied bytes. Use `-Force` only to replace an
existing imported audio BIOS intentionally.

Missing or incompatible FM BIOS files are reported before selecting or
booting a required FM profile. The real BIOS is mapped in a system slot,
leaving both user cartridge slots available. FM and SCC have separate mixer
channels when enabled together.

For **Tiny Magic**, keep the cartridge mapper on **Auto / Konami SCC**, choose
**PSG + FM-PAC/MSX-MUSIC** (or the combined profile), select at least **128 KiB
RAM**, then reboot. **Do not change its cartridge mapper to FM-PAC**: the ROM
banking hardware and the sound hardware are separate choices.

## Disk and tape images

Use **F12 > Media > Disks** or **F12 > Media > Tapes** to select:

- **Drive A** and **Drive B:** raw `.dsk` floppy images.
- **Tape:** an MSX `.cas` cassette image.
- **Rewind tape:** return the current tape to its beginning.

Put your legally obtained images anywhere on the FAT32 card, for example
`msx\disks` and `msx\tapes`. The browser traverses folders and matches file
extensions without regard to case. WAV audio recordings and compressed
archives are not supported.

Supported disks use 80 tracks, nine 512-byte sectors per track, and either
one side (360 KiB / 368640 bytes) or two sides (720 KiB / 737280 bytes).
Forty-track double-sided 360 KiB images and other geometries are not
supported. CAS images must start with the standard MSX cassette marker
`1F A6 DE BA CC 13 7D 74`; raw binary files renamed to `.cas` are rejected.

Select a drive or tape row and press Enter to browse. Select **<Eject media>**
or press Delete on the row to remove its image. When a machine is running,
successful attachments take effect when you resume emulation; no reset is
needed for disk swaps or loading a new tape. Press F12 from either media
page to return directly to the running machine. Failed attachments leave the
previous media selected. When no machine is running, selections apply at
the next cold boot.

Disk images are **read/write**: Disk BASIC commands such as
`SAVE "A:PROGRAM.BAS"` or `SAVE "B:PROGRAM.BAS"`, and supported disk software,
write directly to the attached `.dsk` file on microSD. Each completed sector
is flushed and synchronized before success is reported. No extra F12 save
or ejection is needed to persist disk contents. Saving boot defaults only
remembers media paths; choosing "Reboot without saving" does not undo disk
writes.

**Back up your disk images before using them.** Files/SD cards that cannot be
opened for writing are rejected rather than silently mounted read-only.
Use separate images for A and B; mounting the same file in both drives is
rejected to prevent conflicting cached copies. Keep the card inserted, and
do not power off during a disk operation. A power failure or SD write error
can leave a partial sector or filesystem update, as on a real disk.
Write failures are reported to MSX software and on UART; further writes to
that drive are blocked until you reattach its image. Check/recover the image
on a PC if an operation failed.

Disk formatting and creating new blank images are not implemented; attach
an existing, correctly formatted 360/720 KiB image. CAS tapes remain
**read-only**, and cassette recording is not supported. Attaching an
image does not automatically issue `LOAD`, `RUN`, or a reset.
For a cassette BASIC program use the appropriate command, for example
`CLOAD` or `LOAD "CAS:"`, followed by `RUN`. Binary tapes typically use
`BLOAD "CAS:",R`; follow the software's original loading instructions.
Rewind before reloading the same tape after reaching its end.

Use **Save BIOS + slots + media as boot default** to retain image paths
across restarts. Only paths are saved, not tape position or disk contents;
tapes start at the beginning on a new boot. Older saved configurations
are preserved with the new media assignments initially empty.
Use **Boot BIOS + slots + media (cold reset)** to boot disk software that
requires a reset. Missing saved images produce a recovery error rather
than silently booting with different media.

Leave the SD card inserted while media is attached. F12 BIOS rescanning
does not remount the card while an emulator is running, so an open tape
stream or writable disk handle is not invalidated. Restart the board after physically replacing
the SD card.

Disk BASIC additionally requires a compatible disk BIOS in the selected
profile; a `.dsk` image is not itself that BIOS. If the current machine has
no disk BIOS, F12 reports it instead of claiming a working drive. A BIOS
file added to SD requires a cold boot before disk attachments can be used.

The tested disk BIOS is the user-supplied **Philips NMS8250** 16 KiB dump
`nms8250_disk.rom` (SHA-1 `c3efedda7ab947a06d9345f7b8261076fa7ceeef`).
Copy it unchanged as `DISK.ROM` inside each desired `msx\bios\<profile>`
directory, or add `-DiskBios 'C:\path\to\nms8250_disk.rom'` to the ROM
import command. The importer installs it in every profile imported by that
command; use `-Force` only when intentionally replacing existing imports.
The importer checks size, not compatibility of arbitrary disk BIOS files.

This adds a compatible BIOS-backed disk interface to all six profiles,
including the Panasonic profiles, without replacing their startup/BASIC
ROMs. It does **not** emulate Panasonic's TC8566AF controller or its
built-in applications; the original Panasonic disk ROMs are not supported
replacements. From Disk BASIC, use `FILES "A:"` / `FILES "B:"` to list the
images, and normal disk loading commands such as `LOAD "A:PROGRAM.BAS"`.

## Notes

Once VGA is ready, the startup page shows a progress gauge for menu buffers,
keyboard, audio, microSD, BIOS scanning, saved settings and joystick startup.
The percentage counts completed initialization stages, not elapsed time or
ROM-loading bytes. Warnings remain visible, and the F12 startup selection
window is unchanged.

Rendering adapts to keep emulation near the normal 50/60 Hz rate without
changing the Z80 clock. Sustained overload is checked every five PAL or six
NTSC frames, so a heavy cartridge should no longer spend several seconds
gradually reducing rendering load. F12 resume keeps the learned rendering
level. This can trade display smoothness for speed; it cannot guarantee full
speed for every ROM. The UART `MSX speed:` lines show the measured emulated
and presented frame rates.

This project keeps the original hardware and software assumptions in mind, but it is still an emulator build on modern hardware. That means the goal is a working, usable system rather than a perfect cycle-accurate reproduction of every board detail.

For setup, build, and firmware update details, read the rest of this folder's documentation and use the project notes in the repository as the main reference.
