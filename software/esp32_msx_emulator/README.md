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

## Disk and tape images

Open **F12 > Disks / tape - attach, eject, rewind** to select:

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
needed for disk swaps or loading a new tape. Failed attachments leave the
previous media selected. When no machine is running, selections apply at
the next cold boot.

All images are **read-only**. Disk writes, formatting and cassette saves
are not supported, and original files must not be modified. Attaching an
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
stream is not invalidated. Restart the board after physically replacing
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
