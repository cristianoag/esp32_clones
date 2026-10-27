# ESP32 CP400 / CoCo 2

This is the Prologica CP400 / CoCo 2 firmware for the Retro Hacker Clone
Series ESP32-S3 N16R8 board. It includes MC6809 emulation, VGA, a native
USB keyboard, two USB joystick ports, mono audio, microSD disk images
and an F12 configuration/update menu.

## Hardware

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

Use a directly connected USB HID keyboard. The native USB controller is
reserved for the keyboard; uploads and serial diagnostics use the UART
port. SD uses one-bit SD/MMC, not SPI.

## Build and upload

From this firmware directory:

```powershell
make firmware
make test
make upload
```

PlatformIO is pinned to `espressif32@6.11.0`. `make` finds PlatformIO's
usual Windows virtual environment and exports the firmware version.
Packaging requires Python; host tests require PowerShell and native G++.
Set the upload and monitor ports in [platformio.ini](platformio.ini) to
the board's UART port before using `make upload`.

`make firmware` builds and verifies `dist\ESP32_CP400-1.15.FLH`.
The board definition, libraries and updater are local, so this folder
remains independently buildable. The updater follows the same validated
FLH/OTA implementation as the other clone-series firmware projects.

## SD card and ROMs

Use FAT32. Supply legally obtained raw ROM dumps under `cp400\bios`:

| File | Size | Use |
| --- | --- | --- |
| `extbas11.rom` | 8,192 bytes | Extended BASIC |
| `bas13.rom` | 8,192 bytes | BASIC |
| `cp400dsk.rom` | 8,192 bytes | CP400 disk controller |
| `disk11.rom` | 8,192 bytes | CoCo 2 disk controller |

The first two files and the selected disk controller ROM are required.
Missing, unreadable or wrong-sized ROMs stop startup with a UART error.
Prepare these files before switching to CP400 from another emulator.
The `cp400` folder can coexist with `msx`, `tk` and `apple2` on one card.
ROMs and games are not distributed with the firmware.

Put `.DSK` images at the card root. **F12 > Disk drives** assigns them
to the four virtual drives; Delete ejects the selected disk and saves
the empty assignment. F12 also provides Disk ROM selection, artifact
colors, joystick calibration, reboot and firmware updates.

## Firmware updates and switching emulators

Open **F12 > Firmware update**, browse to a `.FLH` file anywhere on the card
and confirm its name with Y. Enter opens a folder or selects a file; `..`
or Esc returns to the parent folder, and Esc at the root cancels. CP400
accepts CP400, MSX, MSX TurboR, TK95, Apple II, C64, SMS and Macintosh Plus
packages without renaming. The destination emulator starts after installation;
use its Firmware update menu to switch again. Prepare its ROMs first.

The updater checks the shared partition layout, file structure, checksum,
ESP32-S3 image header and 4 MiB image limit before writing the inactive
OTA slot. A second pass detects changes during the update; ESP-IDF
verifies the image before selecting it for boot. Failures are reported
on screen and UART without intentionally restarting or selecting the
failed image. Installation errors remain visible, wrapped across multiple
lines, until Enter or Esc is pressed and released. UART logs show the selected
file, Y confirmation, partition offsets/sizes and boot-slot selection.
Successful updates preserve the source FLH file.
Keep power connected throughout. A checksum is not a signature: use
only trusted firmware for this board.

**Older CP400 installations require one UART upload with `make upload`**
to replace the old small-slot partition table with the shared dual
4 MiB layout. Updating with FLH alone cannot migrate that table.
Boards already running current MSX, TK or Apple firmware on the shared
layout can install this CP400 package directly. Use current builds of
all destinations to retain cross-emulator updates.

### UART recovery without a working keyboard

The board's single USB-C connector exposes its onboard CH343 UART bridge as
COM9 on this setup. No second USB connector or external adapter is required.
Close other serial monitors and run `make upload` from this firmware folder
to install through UART without using F12. The monitor leaves DTR and RTS
deasserted so opening it does not request the ROM download loader.
If automatic upload reset fails, hold BOOT, press and release RESET/EN, release
BOOT, and retry the upload. Release BOOT and reset again for normal startup.
The software USB host no longer claims GPIO0 as an unused blink output.
Version 1.15 also pauses the CPU/software-USB interrupt before OTA metadata
reads, which could otherwise crash with cache disabled before installation
started. An affected 1.14 installation needs UART upload to obtain this fix.
Native keyboard enumeration still needs board testing; a "host started"
message alone does not establish that a keyboard is connected.

See the [firmware change log](docs/log.md) for version history.

`make test` covers nested firmware browsing, pagination and updater failures.
From `software`, `.\Test-FirmwareCompatibility.ps1` verifies all 64
cross-emulator installation combinations using the built FLH files and mock OTA.
