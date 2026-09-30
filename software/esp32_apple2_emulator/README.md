# ESP32 Apple II+ / original Apple IIe

Standalone firmware for the shared Retro Hacker Clone Series ESP32-S3 N16R8
board. Firmware version: **1.02**. The CP400, MSX and TK firmware folders are
not needed to build or run it.

The initial profiles are **generic Apple II+** (48 KiB RAM plus a 16 KiB
language card) and **original Apple IIe** (128 KiB including the extended
80-column card). Both use a cycle-stepped **NMOS 6502**, not a 65C02.
Default: original IIe.

[codesafe/ESP32-VGA_AppleII_Emulator](https://github.com/codesafe/ESP32-VGA_AppleII_Emulator)
is the initial reference. Its II+ hardware is not sufficient for an IIe:
this integration adds IIe banking, internal firmware selection, 80-column
text and double-resolution graphics. The CPU is the licensed chips 6502,
following the TK firmware's cycle-stepped-core pattern. See
[dependency provenance](lib/README.md).

**Brazilian clone profiles are deferred.** TK2000 is not simply an Apple II
ROM replacement; TK3000 IIe/Compact require enhanced-machine and
clone-specific support. MC-4000/Exato and Exato IIe also need their own
verified ROMs and hardware profiles. They are not listed as supported
machines merely because they belong to the Apple family.

## Hardware

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
| Onboard RGB LED, disabled at startup | 48 |

Use the board's **UART connector** for upload and diagnostics. Native USB
is reserved for the keyboard; the board must supply VBUS. Joysticks use
the existing direct low-speed USB transport, not hubs or full-speed pads.

VGA uses a **640x240 framebuffer with hardware line doubling**. The
560x192 Apple picture is centered: 40-column text and 280-pixel graphics
are doubled horizontally; 80-column text and 560-pixel graphics retain
their full horizontal resolution. The F12 UI retains the familiar
320x240 logical layout. Set a widescreen monitor to 4:3/aspect mode.
The pinout and physical VGA timing remain compatible with the shared board.

## Build, test and upload

From this firmware directory:

```powershell
pio run
make test
make firmware
pio run -t upload
```

PlatformIO is pinned to `espressif32@6.11.0`. `make` finds PlatformIO's
usual Windows virtual environment. Native tests require G++ and
PowerShell on PATH. All libraries and the board definition are local.

`make firmware` packages and verifies `dist\ESP32_APPLE2-1.02.FLH` using
the shared decimal checksum + `-~` + ESP32-S3 application format.
Install over UART first to install the dual 4 MiB OTA partition layout.
Later updates use **F12 > Firmware update** and an
`ESP32_APPLE2-*.FLH` file on SD. Other emulator filenames are rejected.
The checksum is **not a signature**; use only trusted Apple firmware.
Renaming a different firmware does not make it compatible. Keep power
connected throughout the update.

## SD card and ROMs

Use FAT32 and put `sdcard\apple2` at the card root. It can coexist with
`cp400`, `msx` and `tk`:

```text
apple2\
  bios\
    apple2plus.rom    12,288 bytes, D000-FFFF
    apple2e.rom       16,384 bytes, C000-FFFF, ORIGINAL IIe
    disk2.rom            256 bytes, slot 6, 16-sector Disk II P5 ROM
  disks\
    your-disk.nib    232,960 bytes
```

Supply legally obtained raw binary dumps. No ROMs, character ROMs, games
or disk images are distributed. Only the selected machine ROM and the
Disk II ROM are required. A combined II+ dump must contain the six
2 KiB ROMs in ascending CPU-address order. The IIe dump must contain the
two 8 KiB halves in ascending CPU-address order. An enhanced IIe ROM is
the same size but requires a 65C02 and is **not supported**.

The import helper checks sizes and verifies copies:

```powershell
.\tools\Import-Roms.ps1 -Destination .\sdcard `
    -IIPlusRom 'C:\roms\appleII+.rom' `
    -IIeRom 'C:\roms\appleIIe.rom' `
    -DiskRom 'C:\roms\diskII.rom'
```

Either machine ROM argument can be omitted. Size validation does not
identify arbitrary ROM contents. Configuration has separate remembered
ROM paths per model and a shared slot-6 ROM path. A file browser can
select ROMs elsewhere on the card.

Without a disk in drive 1, the real Disk II boot firmware waits for media.
Use **F12 > Warm reset** to enter BASIC after the initial boot. To boot a
disk, mount it in drive 1 and select cold boot.

From the BASIC prompt, `PR#6` also boots drive 1. Disk II emulates the
hardware's one-second motor-stop delay, allowing DOS to keep the drive
spinning between sector reads. Firmware 1.00 stopped it immediately,
causing repeated spin-up waits that made games such as Karateka appear
frozen. Update to 1.01 or later if you encounter that symptom. Loading
still takes emulated disk time rather than starting instantly.

## F12 workflow

F12 pauses the CPU, disk emulation and sound. Esc goes back one page;
F12 resumes from ordinary submenus. Confirmations default to cancellation:
**Y** confirms; **N/Esc/F12** cancels.

| Page | Features |
| --- | --- |
| Configuration / ROM | II+/IIe, machine and Disk II ROMs, sound/volume, auto boot, frame skipping, color/green monitor, save/reboot |
| Media / Disk II drives | Mount two read-only NIBs, Delete to eject, save complete boot configuration, reboot with/without saving |
| USB joysticks | Enable/disable each port, seven-step calibration, live input, clear calibration |
| Warm reset | Retain RAM and mounted disks; reset CPU and soft switches |
| Cold boot | Validate selected files, apply pending machine/ROM selection, clear RAM and reset disk heads |
| Retry microSD | Retry an initially failed mount |
| Firmware update | Browse, confirm, validate, install and restart |
| Hardware / help | Wiring, controls and boot instructions |

Arrows navigate; Enter selects; Left/Right changes settings. Browsers
traverse directories, filter extensions case-insensitively and use
Page Up/Down for ten-entry pages. Unsupported/overlong paths are reported.
There is a 2.5-second F12 window before automatic boot.

Machine and ROM changes are **pending until cold boot**. Resuming and warm
reset keep the running ROM/model. Disk mount/eject takes effect on resume
without rebooting; inserting a boot disk alone does not restart the CPU.
Failed validation preserves the old attachment and running state.

Saving validates the selected boot files and stores both machine ROM
paths, slot ROM, both disk paths, sound, volume, auto boot, frame skipping,
monitor color and joystick enable flags in Apple-specific NVS.
Calibration is stored separately in its own Apple namespace.

Keep SD inserted. Restart the board after replacing a mounted card.
Initialization, SD, allocation and NVS failures are reported on VGA/UART.

## Keyboard, paddles and sound

Keyboard input uses the Apple ASCII latch and strobe, with queued press
edges and key repeat. Caps Lock starts **on**. Shift/Caps controls letter
case; Ctrl generates control codes. The layout is US ASCII.

| USB control | Apple action |
| --- | --- |
| Enter / keypad Enter | Return |
| Backspace / Left | Control-H |
| Right / Up / Down | Control-U / Control-K / Control-J |
| Esc / Tab / Delete | Escape / Tab / DEL |
| Left Alt / Right Alt | Open Apple / Solid Apple (pushbuttons 0/1) |
| Shift | Also exposes the third pushbutton input |
| F12 | Host menu only |

Calibrate each USB pad using neutral, up, right, down, left, A and B.
Port 1 drives paddles 0/1 and pushbuttons 0/1; port 2 drives paddles 2/3
and the shared third pushbutton (either fire button). Directional input
maps to minimum/center/maximum paddle positions, not continuous analog
axes. A port's disabled setting returns its joystick input to neutral.

Speaker soft-switch accesses generate 22,050 Hz mono samples. The
shared GPIO47 PWM driver implements volume, pause/mute and idle silence.

## Supported media and limitations

- Two **read-only 35-track NIB** drives in slot 6. Images must be exactly
  35 x 6,656 bytes and contain nibblized disk bytes with bit 7 set.
  Write protection is reported to the emulated controller; writes never
  modify the image or SD. No `.dsk`, `.do`, `.po`, WOZ, cassette or snapshots.
- 40/80-column text, inverse/flashing characters, both display pages,
  lo-res, hi-res, mixed modes and IIe double lo-res/hi-res.
  Text uses the existing Adafruit ASCII font fitted to 7x8 cells, **not**
  an Apple character-ROM reproduction. No MouseText or localized clone glyphs.
- IIe main/aux read/write banking, ALTZP, 80STORE, language-card banks,
  internal Cx/C8 ROM switching and original 80-column ROM entry points.
- NTSC-like fixed CPU/frame timing. Video is rendered at frame boundaries;
  artifact colors, floating-bus timing and disk rotation are approximations.
  No claim of cycle-perfect video, copy-protection compatibility or full
  hardware equivalence. The 6502 itself is cycle-stepped.
- No enhanced IIe/65C02, IIc/IIgs, CP/M/Z80 card, Mockingboard, printer,
  serial cards, extra RAM cards or verified Brazilian clone profiles.

## Performance diagnostics

The CPU targets about 60 emulated frames per second. A disk that takes
35 emulated seconds takes about 70 wall-clock seconds at 30 fps, or
105 seconds at 20 fps. Firmware 1.01 was measured on the board at about
20 fps rendering every frame and 30.6 fps rendering every third frame.
Those rates are slow emulation, not evidence that the CPU has frozen.

Version 1.02 places the 6502 decoder and memory-access hot path in
internal instruction RAM, avoids unused floating-bus calculations during
disk reads, renders scanlines in internal RAM before copying to PSRAM,
and skips unchanged display frames. Flashing text, video-mode/page
changes and writes to either display bank still trigger redraws. It keeps
the 640x240 framebuffer and full-width 80-column output.

UART logs now split average time per emulated frame into **CPU**, **video**,
**copy** and **audio** milliseconds, and include actual redraw count,
CPU PC, drive-1 track and elapsed emulated time. Video/copy averages include
skipped frames, not just rendered ones. Startup also reports internal
heap headroom. Capture several steady-state lines; the initial boot phase
is not representative of game speed. The new optimizations require fresh
on-board measurements before claiming full-speed operation.

## Validation

`make test` uses synthetic data only: CPU decimal arithmetic, bank
selection, language-card write protection, keyboard/paddles, bounded NIB
access, write-protected media, video layout, audio sample counts, settings,
joystick calibration and FLH validation/failure paths.

With user-supplied ROMs in the default SD layout, run `make test-roms`.
Alternative paths can be passed to `tools\Test-Roms.ps1` with
`-IIPlusRom`, `-IIeRom` and `-DiskRom`. This boots both profiles, evaluates
BASIC expressions, checks actual output in 80-column memory, and boots
an original synthetic NIB sector through the Disk II ROM.

An optional test uses your local Karateka NIB (not distributed):

```powershell
.\tools\Test-Roms.ps1 -KaratekaDisk .\sdcard\apple2\disks\karateka.nib
```

This validates the reported disk revision by SHA256, then checks cold
boot and `PR#6` on both machines. The intro must be visible within 60
emulated seconds, and Space must advance into the game within a further
60 seconds. It does not change the disk image.

Host regression tests, ROM smoke tests and the ESP32-S3 build/package
were verified during integration. Physical VGA/USB/audio/SD operation
and sustained real-time speed still require testing on the board.
