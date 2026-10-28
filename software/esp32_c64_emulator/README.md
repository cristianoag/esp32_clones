# ESP32 Commodore 64

Standalone **1.04** firmware for the shared Retro Hacker Clone Series
ESP32-S3 N16R8 board. Other emulator folders are not required to build it.
The CPU, CIA, VIC-II and SID are adapted from
[retroelec/T-HMI-C64](https://github.com/retroelec/T-HMI-C64), revision
`dea1a82057882e46700c27d7df3f59dd481ae974`.

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

Upload and monitor through the **UART connector**. Native USB is reserved
for a boot-protocol keyboard; supply USB VBUS from the board. The two
software-USB joystick ports support direct low-speed devices, not hubs or
full-speed controllers. Calibrate each pad in F12 before use.

VGA uses the shared 320x240 logical framebuffer and hardware line doubling.
The 320x200 C64 picture is centered with 20-pixel top/bottom borders.
The 16-color palette is quantized to the board's RGB222 wiring.
Use a monitor's 4:3/aspect setting. Emulation targets PAL, 312 raster lines
and 50 frames per second; this is not a claim of measured hardware speed.

## Build, test and package

From this directory:

```powershell
make test
make test-roms
make firmware
make upload
```

PlatformIO is pinned to `espressif32@6.11.0` / Arduino 2.x. `make` locates
PlatformIO's normal Windows virtual environment. Host tests require G++
and PowerShell. Board definitions and libraries are local.

`make test` uses synthetic, original test fixtures; no Commodore ROM bytes
are embedded in the firmware or test suite. `make test-roms` additionally
boots **your local ROMs** into BASIC READY and loads/runs a tokenized BASIC
program. To use ROMs outside the SD staging folder:

```powershell
.\tools\Test-Roms.ps1 -RomDirectory 'C:\roms\c64'
```

The smoke directory must contain the three canonical filenames below.
Host tests cover banking, CPU execution, VIC/CIA registers, SID audio,
keyboard scanning in both directions, simultaneous joystick input,
PRG/D64 validation, settings, calibration, mute and FLH error handling.
CPU regressions also check the NMOS original-value and final-value writes
for all 66 memory read-modify-write opcodes, unchanged instruction cycle
counts, accumulator forms and raster-interrupt acknowledgment.
From the parent software directory, run
`.\Test-UsbJoysticks.ps1 -Projects esp32_c64_emulator` to also exercise
production USB control handling, delayed configuration status/replay,
bounded timeouts, STALL responses and packet capture on both connectors.
Physical VGA, audio, USB enumeration, sustained emulation speed and actual
flash/reboot switching still require on-board validation.

## SD card and ROMs

Use FAT32. Put `c64` at the card root, alongside any other emulators:

```text
c64\
  bios\
    basic.rom       8192 bytes
    kernal.rom      8192 bytes
    chargen.rom     4096 bytes
  programs\
    your-program.prg
    your-disk.d64
```

Supply legally obtained, raw binary C64 ROMs. Padded EPROM programmer
images are not accepted, even if their filename identifies the right
chip. Typical original chips are BASIC 901226-01, KERNAL 901227-03 and
character ROM 901225-01. No ROMs, games or disk images are distributed.
The whole local `sdcard` staging folder and ROM/bin files are git-ignored.

```powershell
.\tools\Import-Roms.ps1 `
    -BasicRom 'C:\roms\basic.bin' `
    -KernalRom 'C:\roms\kernal.bin' `
    -CharacterRom 'C:\roms\characters.bin'
```

The importer validates all sizes before copying and verifies SHA-256
hashes. `-Destination 'E:\'` writes directly to a card. File-size checks
do not establish a ROM's identity or legal status. All three ROMs are
required. Select alternative `.rom` files using F12 > Configuration / ROM.
ROM selection stays **pending until cold boot**; reset/resume retains
the running ROMs. Failed file validation leaves the running machine intact.

Keep the card inserted. Retry microSD handles an initially failed mount;
restart the board after replacing an already mounted card.

## F12 menu

F12 pauses emulation and mutes/flushes audio. Arrows select, Enter applies,
Esc goes back, and F12 resumes from ordinary submenus. Destructive actions
require **Y**; **N/Esc/F12** cancels without resuming through confirmation.
File browsers support directories, case-insensitive extensions and
Page Up/Down. A 2.5-second startup F12 window precedes automatic boot.

| Page/action | Behavior |
| --- | --- |
| Configuration / ROM | Select BASIC, KERNAL and character ROMs; sound, volume, auto boot, frame skipping, joystick port swap; save and reboot options. |
| Media / PRG and D64 | Load a PRG, choose a PRG inside a D64, or queue BASIC RUN and resume. |
| USB joysticks | Enable each physical port, seven-step calibration, live status, clear calibration. |
| Warm reset | Reset CPU/devices using running ROMs, retaining RAM. |
| Cold boot | Validate and apply selected ROMs, clear RAM and reset devices. |
| Retry microSD | Retry an initially failed SD mount. |
| Firmware update | Browse compatible FLH packages, confirm, validate, install and restart. |
| Hardware / help | Shared wiring and C64 control reference. |

Boot settings use the independent `c64` NVS namespace. Joystick
calibration uses `c64-joystick`; other emulators' settings are not changed.
NVS writes and OTA pause software-USB traffic, following the other firmwares.
Allocation, SD, NVS, CPU halt and OTA failures are reported on VGA/UART.

## Keyboard and joysticks

Keyboard input is a simultaneous active-low 8x8 C64 matrix, not an ASCII
latch. Both CIA scanning directions work. The C64 KERNAL supplies key repeat.
US PC punctuation is translated to the corresponding C64 chord; PC symbols
without a direct C64 equivalent are not synthesized as unrelated characters.

| USB control | C64 action |
| --- | --- |
| Letters / digits / US punctuation | Corresponding C64 keys/chords. |
| Shift | C64 shift; some US punctuation is remapped to a different C64 chord. |
| Caps Lock | Shift Lock toggle, initially off. |
| Ctrl | CONTROL. |
| Alt / GUI | Commodore key. |
| Esc | RUN/STOP. |
| F11 | RESTORE NMI, once per press edge. |
| F1-F8 | C64 F1-F8; even functions use Shift plus the preceding odd key. |
| Enter / keypad Enter | RETURN. |
| Backspace / Delete | INST/DEL; Shift inserts. |
| Home | CLR/HOME; Shift clears. |
| Arrows | C64 cursor keys, including shifted left/up. |
| Backtick | C64 left-arrow key. |
| Backslash | C64 pound key. |
| F12 | Host menu only. |

USB joystick 1 maps to C64 port 1 and joystick 2 to port 2. Many games
expect **port 2**; enable **Swap joystick ports** if using only the first
physical port. Directions and button A map to the five active-low C64
lines. Button B is captured by calibration but has no extra C64 fire line.
Both ports and the keyboard stay active together; keyboard scans are
not suppressed when a joystick is held. Calibrations are bound to the
device identity/report format and are released on disconnect or mismatch.

Some disk programs begin with an intro or trainer that expects keyboard
input before the game starts. The CPU preserves NMOS read-modify-write bus
writes so common raster-interrupt acknowledgment sequences do not leave
these screens stuck in repeated interrupts.

Control requests without a data stage use bounded 100-poll status and replay
windows rather than four polls, allowing controllers time to finish configuration.
UART identifies the failed request and enumeration stage, including replay/ACK
failures; a configuration-transfer failure is not reported as an unsupported HID
report descriptor. If enumeration fails, unplug and reconnect the controller.

## Programs and limitations

Wait for **READY.** before loading a program from F12. The media loader
copies a PRG to its two-byte load address; BASIC programs at `$0801` also
update BASIC's end pointers. Resume and type `RUN`, or choose
**Queue BASIC RUN and resume**. Machine-code programs may need `SYS`
with the program's documented entry address. Loading replaces the
specified RAM range; it does not reset an already running program.

D64 support is **read-only program extraction**, not an attached drive.
Only 35-track images (174848 bytes, or 175531 with an error table) and
closed PRG entries are supported. Sector links, cycles, terminal lengths,
destination bounds and supplied error-table statuses are checked before
changing emulated memory.

This initial port does **not** implement a 1541 CPU/IEC bus, in-emulator
`LOAD`/`SAVE` disk operations, multi-load disk games, disk writes, tape
formats, CRT cartridges, REU, snapshots, NTSC timing, BLE or web input.
SID synthesis follows upstream's approximation, without analog filter
accuracy. CPU execution is instruction-stepped; VIC timing follows the
upstream game-compatible 63-cycle line budget without badline CPU stalls.
Raster effects and software requiring exact bus timing are not guaranteed.

## Interchangeable firmware

`make firmware` builds and verifies `dist\ESP32_C64-1.02.FLH`:
the shared decimal checksum + `-~` + ESP32-S3 application format.
Copy it and other trusted emulator packages to the FAT32 card root.
In any current firmware choose **F12 > Firmware update**, select the
destination package without renaming, confirm and keep power connected.
C64 retains the same updater for switching back.

Use a current dual-4-MiB OTA layout: app0 at `0x10000`, app1 at `0x410000`.
Old partition tables require a **one-time UART upload**; FLH cannot replace
a partition table or bootloader. Older filename-restricted updaters must
first install a current build of their own emulator.
Prepare the destination emulator's SD files before switching.
Checksums detect corruption, **not authenticity**; install only trusted
firmware for this board. The original package is preserved on SD.

After packaging every firmware, run the repository's
[cross-firmware compatibility test](../Test-FirmwareCompatibility.ps1)
to exercise all eight updaters against all eight destination packages
using mock OTA storage. This does not substitute for physical flash tests.
