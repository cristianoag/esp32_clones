# ESP32 Microdigital TK95 / TK90X

Standalone TK95 and **48 KiB TK90X** firmware for the shared Retro Hacker
Clone Series ESP32-S3 N16R8 board. The CP400 and MSX firmware folders are
not required to build it. Firmware version: **1.00**.

This is a focused implementation, not a port of all ESPectrum features.
[ESPectrum](https://github.com/EremusOne/ESPectrum) is the reference for
Microdigital frame timing, floating-bus behavior and ROM language selection.
The CPU is the permissively licensed, cycle-stepped
[chips Z80](https://github.com/floooh/chips). No ESPectrum GPL source or
ROM bytes are linked into the firmware. See [dependency provenance](lib/README.md).

## Hardware

The pinout is the same as the CP400 and MSX projects:

| Function | GPIO |
| --- | --- |
| VGA red low/high | 5 / 4 |
| VGA green low/high | 7 / 6 |
| VGA blue low/high | 9 / 8 |
| VGA HSYNC / VSYNC | 2 / 1 |
| Native USB keyboard D- / D+ | 19 / 20 |
| Low-speed USB joystick 1 D- / D+ | 15 / 16 |
| Low-speed USB joystick 2 D- / D+ | 17 / 18 |
| One-bit SD/MMC CMD / CLK / D0 | 38 / 39 / 40 |
| Mono PWM audio | 47 |
| Onboard RGB LED, disabled after VGA setup | 48 |

Use the board's **UART connector** for uploading and diagnostics, not the
native USB keyboard connector. The board must provide USB VBUS. Joysticks
use the existing software low-speed USB transport: not USB hubs, Bluetooth,
or arbitrary full-speed gamepads.

VGA uses the existing 320x240 framebuffer and hardware line doubling.
The 256x192 TK picture is centered with its border, without horizontal
stretching. Select 4:3/aspect mode on a widescreen monitor. The physical VGA
signal stays at the board's standard timing; the selectable emulated
50/60 Hz ULA timing does not reconfigure the monitor.

## Build and upload

From this firmware folder:

```powershell
pio run
make test
make firmware
pio run -t upload
```

`make` automatically finds PlatformIO's usual Windows virtual environment.
`make test` requires native G++ on PATH and PowerShell. PlatformIO is pinned
to `espressif32@6.11.0`, as in the MSX firmware.

`make firmware` builds and verifies `dist\ESP32_TK95-1.00.FLH`: the shared
decimal checksum + `-~` + raw ESP32-S3 application format. First installation
must use UART so the bootloader and dual 4 MiB OTA partition layout are
installed. Subsequent updates use **F12 > Firmware update** and any
compatible `.FLH` package for CP400, MSX, TK95 or Apple II on SD, without
renaming. Prepare the destination emulator's ROMs before switching.
Put packages at the card root so CP400's file picker can also find them.
After restart, use the destination's Firmware update menu to switch again.

The updater validates the package before writing, verifies a second pass,
and selects the inactive OTA slot only after ESP-IDF image verification.
Keep power connected throughout. A filename/checksum is **not a signature**;
only install trusted firmware for this board. The original FLH is preserved.
An older TK updater must first install the current TK package to remove
its filename restriction. Older CP400 layouts need a one-time UART upload
of a current build; FLH cannot change the partition table. Use current
builds for every destination to retain cross-emulator updates.

## SD card and ROMs

Use FAT32. Copy the `sdcard\tk` folder to the card root, alongside `msx`
if you use both firmwares on the same card. Default paths:

```text
tk\
  bios\
    tk95.rom
    tk90.rom
```

Supply legally obtained **raw binary, exactly 16,384-byte ROMs**. No ROMs
are distributed with the project. Git ignores ROM/media files under
`sdcard`. A downloaded HTML page or C header renamed to `.rom` is not a
ROM dump and will not boot.

Either ROM can be selected elsewhere on SD through **F12 > Configuration /
ROM**. The firmware remembers a separate ROM path for each model. It checks
size, not the identity or compatibility of an arbitrary 16 KiB image. TK90X
v1/v2/v3 variants can be supplied as separate files; a selected ROM's custom
extensions are not automatically added as emulated hardware.

The default ROM paths on the card are `/tk/bios/tk95.rom` and
`/tk/bios/tk90.rom`. Move an older root-level `bios` folder under `tk`.
Saved settings from the previous layout automatically migrate the two old
default ROM paths; custom ROM selections, tape paths and other settings
remain unchanged. Save the boot configuration in F12 to persist the migrated
paths. Browsers can still access the whole card; tapes and snapshots may be
organized under `tk` or selected elsewhere.

**ROM language pin** selects the Portuguese/Spanish hardware input used by
the original TK95 and TK90X v1/v2 ROMs. It does not translate the F12 menu
or override the language embedded in a custom ROM. Default: Portuguese.
The default machine is TK95, 60 Hz; only the selected machine's ROM is
required to boot.

Leave the card inserted. F12 can retry an initially failed SD mount.
Restart the board after physically replacing a mounted card.

## F12 menu

F12 is reserved exclusively for the host menu. It pauses CPU, tape and
audio without resetting the machine. Esc moves back one page; F12 resumes
from normal submenus. Destructive confirmations default to cancellation:
press **Y** to confirm or **N/Esc/F12** to cancel.

| Page/action | Available controls |
| --- | --- |
| Configuration / ROM | TK95/TK90X, ROM browser, 50/60 Hz Microdigital ULA, sound, volume, auto boot, render every 1/2/3 frames, ROM language pin |
| Configuration / Media | Save full boot configuration, reboot and save, reboot without saving |
| Media | Attach/play/stop/rewind/eject TAP, load 48K SNA/Z80 snapshots |
| USB joysticks | Per-port Disabled/Kempston/Sinclair 1/Sinclair 2/Cursor, seven-step calibration, live decoded input, clear calibration |
| Warm reset | Reset CPU, retain RAM and running machine configuration, rewind/stop tape |
| Cold boot | Apply selected configuration, clear RAM, start selected ROM, rewind/stop tape |
| Retry microSD | Retry initial mount without disrupting an already mounted card |
| Firmware update | Browse, confirm, validate, install FLH, restart |
| Hardware / help | Hardware summary, key mapping and media instructions |

Arrows navigate. Enter selects. Left/Right changes configuration values.
Browsers traverse folders, use case-insensitive extensions, paginate ten
entries, and support Page Up/Down. Names appear in filesystem order.
Paths longer than 239 bytes or containing unsupported components are
reported and omitted rather than silently truncated.

Model, ROM, ULA timing and ROM language changes are **pending until cold
boot**. Resuming preserves the running machine. Sound, volume, rendering
interval and joystick interfaces apply on resume. Warm reset still uses
the running model, not pending model/ROM selections.

**Save complete boot configuration** saves model, both ROM paths, timing,
language, sound/volume, auto boot, rendering interval, both joystick
interfaces and tape path. It validates the selected boot ROM and tape
before saving. Reboot without saving does not overwrite these defaults.
Joystick calibration is saved separately and immediately in TK-specific
NVS; it does not alter MSX calibration.

Invalid saved settings, missing ROMs/media, malformed files, allocation
failures and SD/NVS errors are reported on screen and UART. Failed media
or snapshot validation retains the previous attachment/machine state.
There is a 2.5-second F12 interception window before automatic boot.

## Keyboard and joysticks

Letters and digits map to the original TK keyboard matrix, not to an
ASCII text injector. Original Spectrum/TK keyword entry therefore applies.
For example, the standard BASIC `LOAD` keyword is on **J**.

| USB key | TK action |
| --- | --- |
| Either Shift | CAPS SHIFT |
| Ctrl / Alt | SYMBOL SHIFT |
| Enter / keypad Enter | ENTER |
| Esc | CAPS SHIFT + SPACE / BREAK |
| Backspace / Delete | CAPS SHIFT + 0 |
| Caps Lock | CAPS SHIFT + 2 |
| Arrows | CAPS SHIFT + 5/6/7/8 |
| F12 | Host menu; never enters the TK matrix |

Common unshifted punctuation keys provide SYMBOL SHIFT combinations.
This is not full PC-layout/localized text entry: use original TK key chords
for symbols that depend on the ROM's keyboard layout.

Calibrate each USB joystick under F12 using neutral, up, right, down, left,
fire A and fire B. Keep the requested control held until the next prompt.
The transport retains the shared board's eight-byte report limit. Select
the matching joystick interface in the game. Standard TK joystick
interfaces expose **one fire button (A)**; B is captured for the shared
calibration model but is not mapped to a second emulated fire button.
Two pads assigned to Kempston merge their inputs; use Sinclair 1/2 for
separate keyboard-mapped players.

## Tape and snapshots

**TAP:** read-only, standard pilot/sync/data pulses, played through the
emulated EAR input at real emulated speed. Blocks require a valid length
and XOR checksum. Images are limited to 4 MiB and staged in RAM; attachment
does not start playback or type a BASIC command.

1. Boot the appropriate ROM and attach a TAP under F12 > Media.
2. Return to BASIC and enter `LOAD ""`.
3. Open F12 > Media, choose **Play tape**, then resume.
4. Use Stop/Rewind to retry. At end of tape, rewind before playing again.

F12 pauses tape position. Rewind and cold/warm reset stop the tape.
Only its path is saved, not tape position. Custom turbo/protected tape
loaders are not certified; TZX, WAV, recording and instant/flash loading
are outside this release.

**Snapshots:** boot the desired TK ROM first, then load from F12 > Media:

- 48K `.sna` (49,179 bytes), restoring PC from the saved RAM stack.
- `.z80` v1, raw or compressed; v2/v3 plain 48K, raw or compressed pages.

128K, SamRam and peripheral-specific snapshot hardware modes are rejected.
Malformed compression, duplicate/missing pages and invalid SNA stacks
are rejected before live RAM/registers change. Snapshots restore the
software state, not a ROM, model, joystick setting or tape position.
Snapshot saving is not implemented.

## Validation and limitations

Host tests exercise synthetic Z80 programs (including indexed/CB operations,
interrupts and ROM write protection), both complete raster sizes, flash
attributes, audio sample budgets, input matrices, TAP pulse counts and EOF,
snapshot formats/rejection, settings, calibration, quiet audio and OTA
failure paths. They contain no commercial ROM data.

The core uses 228 T-states per line, 262/312 lines and a 32-T-state frame
interrupt. Frame targets follow ESPectrum (16,707/19,895 microseconds).
Rendering samples eight-pixel character groups; this release is **not
certified cycle-accurate** for multicolor, snow or other raster-sensitive
demos. Contention and floating-bus support do not imply full ESPectrum
compatibility. AY audio, Betadisk, other machine models, hardware tape
input, mouse and ESPectrum's extended snapshot formats are not included.

Monitor UART `TK speed:` while testing actual games. Rendering every second
or third frame reduces VGA work without changing the emulated CPU clock.
Host tests and a successful ESP32 build do not establish real-board FPS,
electrical behavior or compatibility with every ROM/game.

With your raw ROMs in `sdcard\tk\bios`, run **`make test-roms`** for the
optional, local-only integration tests. They boot each model at both
50 and 60 Hz, enter `PRINT 1+1` through the keyboard matrix, check the
rendered result against that ROM's digit font, and load an original
synthetic BASIC TAP through the ROM's EAR routines. No ROM bytes are
copied into test sources or transmitted elsewhere. These checks passed
with the two user-supplied ROMs during implementation; physical-board
VGA, sound, USB, SD/OTA behavior and game performance remain untested.
