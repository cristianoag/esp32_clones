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
| USB joystick 1 D- / D+ | 15 / 16 |
| USB joystick 2 D- / D+ | 17 / 18 |
| SD/MMC CMD / CLK / D0 | 38 / 39 / 40 |
| Mono PWM audio | 47 |
| Onboard RGB LED (cleared, then disconnected) | 48 |

SD uses one-bit SD/MMC, not SPI. Connect a USB HID boot keyboard directly
to the keyboard port. Upload and UART diagnostics use the board's PC/UART
USB port; the native USB controller is reserved for the keyboard.
The GPIO48 output is **not** an audio channel.

Audio uses GPIO47 only, with an eight-bit 156.25 kHz PWM carrier and the
board's analog output filter. F12 mute and Sound Off ramp the output down
to a steady low level and stop the sample timer; they do not leave a 50%
carrier running. Sustained silence also fades out automatically, and new
sound restarts playback with a short ramp. This reduces firmware-generated
idle switching but does not eliminate noise coupled through board power,
ground, VGA or USB wiring.

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
| `msx\bios\omega` | MSX2+ | 512 KiB | `OMEGA.ROM` (one 256 KiB bank) |
| `msx\bios\expert` | Gradiente Expert 1.1 / MSX1 | 64 KiB | `MSX.ROM` |
| `msx\bios\hotbit` | Sharp Hotbit 1.2 / MSX1 | 64 KiB | `MSX.ROM` |
| `msx\bios\fs-a1wsx` | Panasonic FS-A1WSX / MSX2+ | 64 KiB | `PANASONIC.ROM` (336 KiB) |
| `msx\bios\fs-a1f` | Panasonic FS-A1F / MSX2 | 64 KiB | `PANASONIC.ROM` (208 KiB) |
| `msx\bios\fs-a1fx` | Panasonic FS-A1FX / MSX2+ | 64 KiB | `PANASONIC.ROM` (208 KiB) |

### Panasonic profiles

Import your machine dumps from the Panasonic directory:

```powershell
.\tools\Import-Roms.ps1 -Destination .\sdcard `
    -PanasonicDirectory 'C:\Program Files\openMSX\share\systemroms\systemroms\machines\panasonic'
```

This imports all three profiles. Use `-PanasonicModels FS-A1FX` to select
one, or a PowerShell array such as `-PanasonicModels FS-A1F,FS-A1WSX`.
Replacing an existing profile requires `-Force`.

The importer reads the named `fs-a1f_*`, `fs-a1fx_*` and `fs-a1wsx_*`
BIOS, sub-ROM, Kanji BASIC and Kanji font files. Each profile gets
**one combined `PANASONIC.ROM`**, with the source bytes unchanged:

| Offset | Contents |
| --- | --- |
| `0x00000` | 32 KiB BIOS/BASIC |
| `0x08000` | 16 KiB sub-ROM |
| `0x0C000` | 32 KiB Kanji BASIC/driver |
| `0x14000` | 128 KiB Kanji font (FS-A1F/FX), or 256 KiB (FS-A1WSX) |

These are emulator system-image layouts assembled from your component dumps,
not byte-for-byte copies of a physical Panasonic mask-ROM chip.
Original dumps are only read, never modified. The combined image and its
checksum stay in the ignored SD preparation directory.

Copy the three generated profile folders to `msx\bios` on the FAT32 SD card,
install the updated firmware, and select them through **F12 > BIOS**.
Choose **Boot BIOS + slots (cold reset)** to apply the selection. The existing
save-default option also retains these profiles across power-off.
Firmware updates do not install the ROM files on SD.

This release uses each machine's BIOS/BASIC, startup behavior, cartridge
slots and Kanji ROMs within the emulator's supported machine model. It does
not map the Panasonic Cockpit/word processor firmware or disk BIOS, and does
not emulate their TC8566AF disk controllers, Panasonic firmware mapper,
turbo switch or other built-in applications. FS-A1F is an MSX2, not MSX2+;
its own BIOS determines its startup screen. The WSX/FX MSX2+ startup animation
must not be substituted for the FS-A1F's behavior.

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
The importer preserves the entire selected bank byte-for-byte as one
`OMEGA.ROM` file. It contains the main BIOS/BASIC at `0x00000`, an optional
auxiliary ROM area at `0x08000`, the extension at `0x10000`, and Kanji BASIC
with the original MSX2+ startup animation at `0x14000`, according to
[Omega's slot map](https://github.com/skiselev/omega/blob/master/Mainboard.md#slot-map).
The emulator reads and maps these regions directly from that single file;
it does not create temporary split ROMs. The unselected bank is not imported.
A SHA256 checksum is generated for checking the complete bank copy.

This boots the Omega BIOS using fMSX's MSX2+ machine model. It is **not**
a cycle-accurate emulation of the physical Omega board, its flash banking,
optional user ROMs, or expansion hardware. Kanji BASIC is mapped as part
of the system ROM, but a Kanji font device, disk controller and music
expansions are not added from the unused bank regions.

### Restore the Omega startup logo on an existing SD card

Earlier imports split out only the main BIOS and extension, omitting the
Kanji BASIC region that also contains the original MSX startup animation.
The updated firmware maps it together with the extension from the single
`OMEGA.ROM` bank. It also emulates the MSX2+ reset-status register and the
sprite-collision status polling used by the animation.

The auxiliary area at `0x08000` is erased in the supplied image; that does
**not** mean its startup logo is missing. The logo comes from Kanji BASIC.
The BIOS runs the original logo code; the host does not draw a replacement
splash screen or occupy either cartridge slot.
The supplied bank has been verified to show that logo and then reach BASIC
both in host tests and on the user's board.

The startup renderer now honors SCREEN 6 coarse/fine horizontal scrolling,
two-page wrap and left-edge masking. Sprite collision status follows the
emulated beam instead of immediately re-triggering on the same overlap,
which previously let the BIOS raster loop advance incorrectly. The collision
sprites' SCREEN 6 color pairs are also sampled correctly, removing the
spurious light-blue bar.

Native tests of the corrected original Omega sequence produce 54 distinct
animation images over 177 emulated frames (about 2.95 seconds at 60 Hz).
The main movement interval now has 47 distinct images, rather than only two
in the earlier implementation. The CPU timing is unchanged, and BASIC is
reached at frame 318 even with rendering reduced to 10%. These are host
measurements; smoothness on the ESP32 still depends on its presented frame
rate. No replacement splash image or fixed host-side animation delay is used.

Install the updated MSX firmware, then copy the newly imported
`msx\bios\omega\OMEGA.ROM` onto the SD card beside `profile.ini`.
An FLH update alone does not install BIOS files. To regenerate
the complete profile from your original image:

```powershell
.\tools\Import-Roms.ps1 -Destination .\sdcard `
    -OmegaRom 'C:\path\to\omega_msx2+_all_ntsc.bin' -OmegaBank 0 -Force
```

Replacing the imported profile with `-Force` removes the old generated
`MSX2P.ROM`, `MSX2PEXT.ROM` and `MSX2PLOGO.ROM` files and restores the
default name/model/RAM manifest. Other files are left alone.
Choose Omega and **Boot BIOS + slots (cold reset)** rather than Resume.
If `OMEGA.ROM` is present in an MSX2+ profile, it takes precedence and must
be exactly 256 KiB; a malformed combined image is not silently replaced
with split BIOS files. Other MSX2/MSX2+ profiles can still use the generic
fMSX separate-file convention below. Expert and Hotbit each remain one
32 KiB ROM.

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
the six initial entries. Missing or invalid BIOS files produce an
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
- **USB joysticks - calibrate / test:** configure both gamepads and view
  live directions, buttons, device IDs and raw USB reports.
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

## USB joysticks

Connect one **low-speed USB HID gamepad** directly to each joystick USB
connector. Physical USB joystick port 1 controls MSX joystick 1; physical
port 2 controls MSX joystick 2. Ports are independent: a single pad is
not mirrored to both players. The keyboard remains on its native USB port,
and gamepads do not generate keyboard or F12-menu commands.

These GPIO-wired ports use the board's software USB host, not the native
USB controller. Full-speed/high-speed controllers, USB hubs, XInput and
Bluetooth controllers are not supported. Low-speed reports must fit in
eight bytes. A supported pad must expose controls that can be learned
from a single report stream and a joystick/gamepad HID report descriptor
of at most 255 bytes; the configuration descriptor also has a 255-byte
limit. The host uses the first configuration and first eligible nonboot
HID interface; mixed keyboard/mouse application collections are rejected.
Calibration is not a universal HID parser.
No report layout is guessed: **an uncalibrated pad produces neutral input**.

### Calibrate each pad

1. Open **F12 > USB joysticks - calibrate / test**.
2. Select joystick 1 or 2 with the arrows and press Enter.
3. Follow the seven prompts: release all controls, then hold Up, Right,
   Down, Left, and the buttons you want for fire A and fire B.
   **No diagonal calibration is required.** Diagonals are derived from
   independent axes/direction bits or standard circular HID hat values.
4. At each prompt, hold only the requested control, press **Enter on the
   keyboard**, and keep holding until the next prompt appears. The 600 ms
   capture first allows queued reports to settle; noise is learned only
   while neutral so an intentional control change is not ignored. Use the same
   stick or D-pad throughout. Esc/F12 cancels without replacing the old
   calibration.
5. After successful calibration, check the live U/D/L/R/A/B indicators.
   Both buttons and diagonal movement can be combined. Return with Esc,
   then resume the emulator; a machine reset is not required.

Calibration is saved immediately in NVS, separately for each physical
port and USB vendor/product identity. It survives power-off and is
independent of **Save BIOS + slots as boot default**. A different pad
requires calibration; identical VID/PID models with different report
layouts may need recalibration too. Delete on the selected joystick row
clears its calibration. Unsupported or ambiguous samples are rejected
with an explanation instead of installing a guessed mapping.

Disconnect releases that port's directions and buttons without affecting
the other player. Joystick polling continues while F12 is open, so released
controls do not stay held on resume. For flash writes (saving settings or
installing firmware), the software host is paused and then restarted;
pads may take a moment to reconnect, and input stays neutral until fresh
reports arrive.

If a pad shows no reports, inspect the UART startup log. At the board's
240 MHz CPU clock, the MSX host prints `cycle-timed TX, 160 cycles/bit`.
The transmitter uses absolute CPU-cycle deadlines in IRAM rather than
compiler-dependent NOP delay calibration. It releases the bus after the
end-of-packet J bit so the receiver can listen for the device's reply.
Enumeration progress, failed-control packet results/edge counts, and device
VID/PID messages help distinguish transport failures from unsupported HID
descriptors. Receive sampling also runs in IRAM, with cycle-based inactivity
timeouts and a bounded capture buffer. Missing saved calibration is
normal on first use; the pad must enumerate before F12 calibration can run.
The decoder starts at the first K transition instead of counting preceding
idle-J time as packet data. Like the CP400 transport, a DATA packet is
validated first and its retransmission is promptly acknowledged; only the
validated copy is delivered to calibration. This avoids a late ACK leaving
the pad repeatedly returning its initial neutral report.

Hardware testing with the user's `2e24:386a` gamepad confirmed changing
reports, successful seven-step calibration, and calibration retained after
restart. This does not establish compatibility with every low-speed pad.

To check the MSX BIOS input path in BASIC:

```basic
10 PRINT STICK(1),STRIG(1),STRIG(3),STICK(2),STRIG(2),STRIG(4)
20 GOTO 10
```

`STICK` returns 0 for neutral and 1-8 for the directions. `STRIG(1)` /
`STRIG(3)` are port 1's A/B buttons; `STRIG(2)` / `STRIG(4)` are port 2's.

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

### Emulation speed

The emulated CPU retains its normal clock. To reduce slowdowns, small
CPU-addressed RAM, BIOS and cartridge allocations prefer internal SRAM
while keeping 64 KiB free for board drivers. Larger allocations, including
Omega's 512 KiB RAM, remain in PSRAM; their placement is logged at boot.

When rendering exceeds the frame budget, the firmware automatically draws
fewer frames rather than slowing the emulated clock deliberately. CPU,
VDP state, sound and input still advance on every emulated frame. Drawing
returns towards 100% when there is spare processing time. This can improve
speed at the cost of visual smoothness; it cannot guarantee full speed for
CPU-heavy games or every MSX2 video mode.

Every five active seconds, UART prints a line such as:

```text
MSX speed: 59.9/60 emulated fps, 35.9 presented fps, draw=60%
```

This is an example, not a measured result for your board. The first number
shows actual emulation speed versus the NTSC 60 / PAL 50 fps target; the
presented rate reports display updates. Time spent in F12 menus is excluded.
Use these lines and the game/profile name when reporting remaining slowdown.

Run the ROM importer regression checks without any copyrighted ROMs:

```powershell
.\tools\Test-RomImport.ps1
.\tools\Test-Profiles.ps1  # Requires the existing g++ toolchain on PATH.
.\tools\Test-Settings.ps1
.\tools\Test-FirmwareUpdate.ps1
.\tools\Test-Joysticks.ps1
.\tools\Test-JoystickRuntime.ps1
.\tools\Test-JoystickTiming.ps1
.\tools\Test-Audio.ps1
.\lib\fmsx\tests\host_smoke.ps1 -ProfilesRoot .\sdcard\msx\bios
.\lib\fmsx\tests\animation.ps1
# Optional original BIOS animation trace and captures:
.\lib\fmsx\tests\animation.ps1 -BiosDirectory .\sdcard\msx\bios\omega -Capture -Raster
```

The checks use synthetic byte arrays to cover bank extraction, file sizes,
profile manifests, custom machines, checksums, overwrite protection and
invalid inputs. The native C++ checks exercise the firmware's actual manifest
parser, including malformed and duplicate settings, compact menu geometry,
SD paths, and old/new saved settings. Joystick tests cover report mapping,
calibration rejection, independent ports, saved calibration, disconnects,
and the core's PSG port selection. The firmware updater tests cover the
FLH validation and failure paths. The core regression checks repeated boots,
cartridge slots, allocation failures/recovery, emulated sound, video and
keyboard input. With `-ProfilesRoot`, it also runs all six imported
BIOS profiles for 900 frames and checks for BASIC's `Ok` prompt. The separate
animation tests check beam-timed collision latching and SCREEN 6 scrolling,
including fine/coarse offsets, page wrapping, masking and sprite colors.
For FS-A1FX/WSX animation captures, select their profile directory and add
`-RamPages 4`. FS-A1F is checked as an MSX2 BIOS, not with this MSX2+ harness.
A successful cross-build does not prove VGA timing, USB
enumeration, analog audio quality or emulation speed on the physical board.

This initial port includes BIOS/BASIC and cartridge boot, keyboard, video,
audio, calibrated low-speed USB joysticks, F12 configuration and SD firmware
updates. Disk/tape browsers, save states and physical Omega expansion devices are not
exposed by this firmware. The original core is not the reference project's
ESP32-optimized engine; real-time performance must be measured on hardware.
VGA has a 320x240 framebuffer and six physically wired color bits.

Before treating a board as validated:

1. Upload through UART and confirm VGA output and USB enumeration in the log.
2. Boot each available profile, type `PRINT 2+2`, and verify BASIC prints `4`.
   With the complete Omega bank installed, check that the MSX startup
   logo appears before BASIC.
3. Open F12 while a BASIC program runs, resume it, then cold-boot another ROM.
4. Save a default, power-cycle, and check that the same profile and RAM return.
5. Try Omega `SCREEN 5` and a BASIC `PLAY` command to check graphics and sound.
6. Remove the SD card before startup and confirm the recovery menu remains
   accessible; reinsert it and rescan.
7. Attach and eject a known cartridge in each slot, cold-boot, and verify
   the assignments persist only after saving the defaults.
8. Install a trusted MSX FLH from the update menu and check the reboot;
   cancel another update and verify the paused emulator can resume.
9. Calibrate both low-speed gamepads and run the BASIC test above. Check
   diagonals and both buttons, unplug/reconnect one pad while the other
   stays held, and verify calibration survives a power cycle.

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
