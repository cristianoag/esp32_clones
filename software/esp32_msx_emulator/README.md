# ESP32 MSX / fMSX

MSX1, MSX2, and MSX2+ firmware for the Retro Hacker Clone Series ESP32-S3
N16R8 board (16 MiB flash, 8 MiB OPI PSRAM). This is an independent board
port of Marat Fayzullin's original fMSX 6.0, not a renamed copy of S3-MSX-PC.

## Hardware

The pin assignments match the existing CP400 firmware, not S3-MSX-PC:

| Function | GPIO |
| --- | --- |
| VGA red, low/high bits | 5 / 4 |
| VGA green, low/high bits | 7 / 6 |
| VGA blue, low/high bits | 9 / 8 |
| VGA HSYNC / VSYNC | 2 / 1 |
| USB keyboard D- / D+ | 19 / 20 |
| SD/MMC CMD / CLK / D0 | 38 / 39 / 40 |
| Mono PWM audio | 47 |
| Onboard RGB LED (cleared, then disconnected) | 48 |

SD uses one-bit SD/MMC, not SPI. Connect a USB HID boot keyboard directly
to the keyboard port. Upload and UART diagnostics use the board's PC/UART
USB port; the native USB controller is reserved for the keyboard.
The GPIO48 output is **not** an audio channel.

Keyboard mappings use international MSX physical key positions. Left Alt
is Graph, Right Alt is Code, F6-F10 are Shift+F1-F5, F11 is Select, and
Pause is Stop. Punctuation and accented characters depend on the selected
Brazilian BIOS layout. Keyboard lock LEDs and USB hubs are not supported.

The firmware reuses the existing board definition, VGA, Adafruit GFX and
BusIO libraries from the sibling CP400 project. Keep both firmware folders
in the repository when building. It does not change CP400 firmware.

## Prepare the SD card

Use a FAT32 card. BIOS files are supplied by the user, loaded from SD, and
never compiled into the firmware. From this firmware directory:

```powershell
.\tools\Import-Roms.ps1 -Destination .\sdcard `
    -OmegaRom 'C:\path\to\omega_msx2+_all_ntsc.bin' `
    -ExpertRom 'C:\path\to\expert_1.1_basic-bios1.rom' `
    -HotbitRom 'C:\path\to\hotbit_1.2_basic-bios1.rom'
```

Copy the generated `sdcard\msx` folder to the card root. Alternatively,
use the SD drive root (for example `E:\`) as `-Destination`.
The imported files and build outputs are ignored by Git.

The initial profiles are:

| Profile directory | Machine | Default RAM | BIOS files |
| --- | --- | --- | --- |
| `msx\bios\omega` | MSX2+ | 512 KiB | `MSX2P.ROM`, `MSX2PEXT.ROM` |
| `msx\bios\expert` | Gradiente Expert 1.1 / MSX1 | 64 KiB | `MSX.ROM` |
| `msx\bios\hotbit` | Sharp Hotbit 1.2 / MSX1 | 64 KiB | `MSX.ROM` |

### Omega's two banks

The supplied Omega file is **524,288 bytes**, containing two **different**
256 KiB banks. The default import uses **bank 0**, the first half.
Use `-OmegaBank 1` to choose the second half, and `-Force` when replacing
an already imported profile:

```powershell
.\tools\Import-Roms.ps1 -Destination .\sdcard `
    -OmegaRom 'C:\path\to\omega_msx2+_all_ntsc.bin' -OmegaBank 1 -Force
```

A standalone 256 KiB image is also accepted, with bank 0 only.
Within the selected bank, the importer extracts the 32 KiB main BIOS at
`0x00000` and the 16 KiB extension at `0x10000`, according to
[Omega's slot map](https://github.com/skiselev/omega/blob/master/Mainboard.md#slot-map).
It deliberately does **not** mistake the logo at `0x08000` for the extension.
The unused bank is not imported. The two required BIOS components total
48 KiB; there is no need to store or load the whole flash chip image.
Per-profile SHA256 checksums are generated for checking the copy.

This boots the Omega BIOS using fMSX's MSX2+ machine model. It is **not**
a cycle-accurate emulation of the physical Omega board, its flash banking,
optional logo/user ROMs, or expansion hardware. No optional disk, Kanji,
or music BIOS is taken from the Omega user-ROM regions.

### Add other boot ROMs

Create additional named profiles without rebuilding:

```powershell
# Another MSX1 machine:
.\tools\Import-Roms.ps1 -Destination .\sdcard `
    -BiosRom 'C:\path\to\bios.rom' -Model MSX1 `
    -ProfileId mymsx1 -Name 'My MSX1'

# An MSX2 machine requires both BIOS components:
.\tools\Import-Roms.ps1 -Destination .\sdcard `
    -BiosRom 'C:\path\to\msx2.rom' -ExtensionRom 'C:\path\to\msx2ext.rom' `
    -Model MSX2 -ProfileId mymsx2 -Name 'My MSX2'
```

Use `-Model MSX2+` for an MSX2+ pair. BIOS must be exactly 32 KiB and
extension ROM exactly 16 KiB. These are **system BIOS** profiles, not
game cartridges. Each profile has a small `profile.ini`, for example:

```ini
name=My MSX2
model=MSX2
ram=512
```

Allowed RAM sizes are 64, 128, 256, or 512 KiB. Names must be printable
ASCII, at most 40 characters. Directory IDs are 1-32 letters, digits,
underscores, or hyphens. The F12 menu scans up to 32 profiles, including
the three initial entries. Missing or invalid BIOS files produce an
explicit menu error instead of booting a different ROM silently.

## F12 configuration

Press **F12** at any time to pause the emulated machine and enter the menu.
Press **Esc** or **F12** to restore the previous screen and resume.

- **BIOS:** Left/Right (or Enter) selects a boot profile.
- **Slot 1 / Slot 2:** Enter opens an SD-card browser for cartridge ROMs.
- **RAM on next boot:** select 64/128/256/512 KiB.
- **Sound:** enable or mute audio.
- **Auto boot saved settings:** enable or disable automatic startup.
- **Save BIOS + slots as boot default:** persist profile, both cartridge
  paths, RAM, sound and auto-boot choices in NVS.
- **Boot BIOS + slots (cold reset):** discard the current machine state
  and boot the selected BIOS with both selected cartridges attached.
  This does not implicitly save the default.
- **Rescan SD card / other BIOS profiles:** remount and rediscover profiles.
- **Firmware update from SD:** select an MSX FLH package, confirm, install
  it to the inactive firmware partition and reboot.

Options occupy consecutive eight-pixel text rows, with no blank rows
between them. Selection alone does not reset the running machine. BIOS,
slot and RAM changes take effect on the next cold boot; Resume keeps the
current machine and its cartridges unchanged. Existing saved settings from
before cartridge support are preserved, with both slots initially empty.
On startup there is a 2.5-second opportunity to
press F12 before auto-boot. A fresh installation defaults to Omega bank 0;
missing SD/BIOS/cartridge files leave the menu available for recovery
instead of silently booting with a configured cartridge missing.

## Cartridge ROMs on SD

Copy your legally obtained, uncompressed `.ROM` cartridge files to the
SD card, for example `msx\roms\`. They are separate from the system BIOS
profiles; the BIOS importer is not needed for cartridge files.

1. Press F12 and select **Slot 1** or **Slot 2**, then Enter.
2. Browse folders with Up/Down and Enter. Left or `..` goes to the parent
   folder. PgUp/PgDn changes pages; Esc/F12 cancels without changing the slot.
   File extensions are case-insensitive.
3. Select a `.ROM` file. Repeat for the second slot if needed.
4. Choose **Boot BIOS + slots (cold reset)** to start with those cartridges.
   Choose **Save BIOS + slots as boot default** first if the assignments
   should survive power-off.

Choose **<Eject slot on next boot>** in a slot's browser, or press Delete
there or on the main menu's slot row, to clear an assignment (even if the
card is unavailable). Cold-boot to apply the ejection, and save
the default if it should remain empty at the next power-on.

The slots correspond to emulated MSX primary cartridge slots 1 and 2,
not extra ESP32 connectors. Cartridge startup follows the selected BIOS's
normal initialization rules; assigning two games does not mean both run
at once. The core handles supported plain and bank-switched ROM formats, up to
2 MiB per slot in multiples of 8 KiB, with cartridge headers recognized
by fMSX. Files must be raw ROM images, not ZIP archives or disk images.
The browser is paginated without a fixed file-count limit; full SD paths
must fit in 239 bytes. Unsupported or overlong names are reported.

## Firmware update from SD

Install this firmware through USB/UART first if the currently installed
MSX version does not yet offer an update menu. For later MSX updates:

1. Run `make firmware` and copy `dist\ESP32_MSX-1.00.FLH` to the SD card
   (the root or any folder accessible from the file browser).
2. Open F12, select **Firmware update from SD**, and choose the FLH file.
3. Confirm with **Y**, or select Yes with the arrows and press Enter.
   **N**, Esc, or F12 cancels; Cancel is selected by default.
4. Wait for validation, flashing and automatic reboot. Do not switch off
   the board or remove the card during installation.

Only MSX packages named `ESP32_MSX-*.FLH` are accepted. The updater checks
the package checksum and target image before writing the inactive OTA
partition, checks the written image before selecting it for boot, and
reports failures on screen and UART. The source FLH is kept on SD.
The FLH checksum detects corruption; it is not a digital signature.
Only install packages you trust. Unsaved emulated machine state is lost
on a successful update; save any desired boot defaults before updating.

## Build and upload

PlatformIO is pinned to `espressif32@6.11.0` / Arduino 2.x because the shared
VGA driver uses ESP-IDF 4 APIs. With PlatformIO on PATH:

```powershell
pio run
pio run -t upload --upload-port COM18
pio device monitor --port COM18 --baud 115200
```

Replace `COM18` with your board's UART port. The firmware does not hard-code
a port. The Makefile finds a standard Windows PlatformIO installation
automatically, including its Python interpreter for packaging. Otherwise,
Python 3 must be on PATH (or supplied through `PYTHON`). With `make` on PATH:

```powershell
make firmware
```

`make` (the default target) and `make firmware` build the application, create
`dist\ESP32_MSX-1.00.FLH`, and verify its checksum and byte-for-byte payload.
The FLH format matches CP400: an ASCII decimal sum of the bytes in
`-~` plus the application binary, followed by `-~` and that binary.
No BIOS files are included. The `dist` directory remains ignored by Git.

`make build` or `pio run` builds only the raw application.
`make package` rebuilds and creates the FLH; `make verify` also validates it.
`make upload` uploads through UART. `make clean` removes build output and
the selected FLH file. `OUTPUT_DIR`, `UPDATE_FILE`, and `PYTHON` can be
overridden on the make command line.

Use **USB/UART upload for first installation or switching emulators**.
Matching the CP400 FLH container
format does not make cross-firmware updating supported: do not feed the MSX
FLH to the CP400 F12 updater, or a CP400 FLH to the MSX updater.
The MSX updater requires its own two-slot partition layout. An FLH
contains only the application, not the bootloader or partition table.

## Validation and current scope

Run the ROM importer regression checks without any copyrighted ROMs:

```powershell
.\tools\Test-RomImport.ps1
.\tools\Test-Profiles.ps1  # Requires the existing g++ toolchain on PATH.
.\tools\Test-Settings.ps1
.\tools\Test-FirmwareUpdate.ps1
.\lib\fmsx\tests\host_smoke.ps1 -ProfilesRoot .\sdcard\msx\bios
```

The checks use synthetic byte arrays to cover bank extraction, file sizes,
profile manifests, custom machines, checksums, overwrite protection and
invalid inputs. The native C++ checks exercise the firmware's actual manifest
parser, including malformed and duplicate settings, compact menu geometry,
SD paths, and old/new saved settings. The firmware updater tests cover the
FLH validation and failure paths. The core regression checks repeated boots,
cartridge slots, allocation failures/recovery, emulated sound, video and
keyboard input. With `-ProfilesRoot`, it also runs all three imported
BIOS profiles for 900 frames and checks for BASIC's `Ok` prompt.
A successful cross-build does not prove VGA timing, USB
enumeration, analog audio quality or emulation speed on the physical board.

This initial port includes BIOS/BASIC and cartridge boot, keyboard, video,
audio, F12 configuration and SD firmware updates. Disk/tape browsers,
save states, joystick ports and physical Omega expansion devices are not
exposed by this firmware. The original core is not the reference project's
ESP32-optimized engine; real-time performance must be measured on hardware.
VGA has a 320x240 framebuffer and six physically wired color bits.

Before treating a board as validated:

1. Upload through UART and confirm VGA output and USB enumeration in the log.
2. Boot each of the three profiles, type `PRINT 2+2`, and verify BASIC prints `4`.
3. Open F12 while a BASIC program runs, resume it, then cold-boot another ROM.
4. Save a default, power-cycle, and check that the same profile and RAM return.
5. Try Omega `SCREEN 5` and a BASIC `PLAY` command to check graphics and sound.
6. Remove the SD card before startup and confirm the recovery menu remains
   accessible; reinsert it and rescan.
7. Attach and eject a known cartridge in each slot, cold-boot, and verify
   the assignments persist only after saving the defaults.
8. Install a trusted MSX FLH from the update menu and check the reboot;
   cancel another update and verify the paused emulator can resume.

## Attribution and licensing

- **fMSX 6.0:** Copyright Marat Fayzullin. Original source and terms:
  <https://fms.komkon.org/fMSX/>. Non-profit use requires attribution;
  commercial use requires a separate license from the author. Preserve
  the source headers. The author requests notification of ports/changes;
  contact him before distributing this port.
- **S3-MSX-PC:** <https://github.com/Svarkovsky/s3-msx-pc> was used as a
  reference for the project's goal, not as the source for the combined
  firmware. Its license explicitly acknowledges a proprietary/GPL
  conflict. No retro-go framework or S3-MSX-PC custom drivers were copied.
- **Board integration:** follows this repository's non-commercial terms,
  with CP400 keyboard-driver attribution retained where applicable.
- **VGA/GFX/BusIO:** retain their respective authors and licenses in the
  existing sibling library directories.

No rights to redistribute the user's original MSX BIOS images are granted
by the firmware license. Keep ROM binaries out of commits and releases.
See [the firmware change log](docs/log.md).
