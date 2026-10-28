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
SD/MMC uses one-bit mode at 20 MHz, matching the other Clone Series firmwares
rather than the SD library's 40 MHz default. A failed mount never formats the card.

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
msxturbor/roms/                             optional cartridge .rom files
```

The importer accepts both known ST firmware revisions; select the alternate
with `-FirmwareRom 'C:\path\to\fs-a1st_firmware_other_version.rom'`.
Keep the full 4 MiB dump. The emulated ST firmware mapper exposes its first
256 8 KiB banks, with mirroring as in the openMSX ST definition.
Identical read-only firmware banks share one allocation without changing their
logical bank numbers. This saves about 0.75 MiB of resident memory with the
tested original ST dump, leaving room for a 4 MiB cartridge in 8 MiB PSRAM.
BIOS, opening, disk, Kanji BASIC and MSX-MUSIC regions are mapped from that
image; no additional disk BIOS or extracted sidecar ROMs are needed.
The GT firmware is intentionally rejected.
ROM loading checks the opened file's size without seeking through the dump,
then reads sequentially in chunks of at most 4 KiB through aligned internal
stack memory before copying into PSRAM. It avoids heap-backed stdio buffers,
handles short/interrupted reads and yields between reads to service board tasks.
Failures report the operation, byte offset and OS error rather than just the
file size; the full path is included in the UART diagnostic.

No BIOS or software images are distributed or embedded in the application.

## Build and upload

```powershell
pio run
make firmware
make upload
```

`make firmware` builds and verifies `dist\ESP32_MSXTURBOR-1.04.FLH`.
`make upload` is a UART upload, including the shared partition table.
The Makefile uses PlatformIO's standard Windows virtual environment.

## F12 configuration

F12 pauses the emulated machine and sound, backs up the VGA picture, and
opens the same 320x240-style configuration interface used by the other
firmwares. Use arrows and Enter; Esc or F12 resumes a booted machine.

- **Media:** choose ROMs for **Slot 1** and **Slot 2**, select their mappers,
  and attach/eject raw 360/720 KiB `.dsk` files in **Drive A** and **Drive B**.
  File browsers cache the current 16-entry page: moving its highlight does
  not reread the SD directory. Changing pages/folders or reopening the browser
  refreshes the listing; a reading message is shown during directory scans.
  Enter browses; Delete ejects. ROM and mapper changes apply on cold boot;
  disk changes are live while paused. A failed disk attachment retains the
  old image. One writable image cannot be attached to both drives.
- **Sound:** PSG, built-in MSX-MUSIC, TurboR PCM and cartridge SCC output.
  Enter opens independent switches for **PSG**, **MSX-MUSIC (FM)**,
  **TurboR PCM**, **SCC cartridge** and **Keyboard click**, plus the master
  switch. Left/Right on the main Sound row still toggles master sound.
  **PWM output gain** selects 1x, 2x, 4x or 8x, defaulting to 8x. Gain is
  applied with saturation before rounding to 8-bit PWM, not after quantization:
  the mixer's quiet PSG output otherwise uses only a few duty levels. Older
  settings migrate with 8x gain; source/debug/master choices are preserved.
  Reduce gain if the diagnostic `pwmclip` counter rises significantly.
  Source switches mute output only: guest registers, chip availability and
  cartridge mapping do not change. Changes apply when resuming, without
  rebooting; **Save boot configuration** persists them. Older saved settings
  that predate source switches migrate with every source enabled; newer saved
  source choices and the master setting are preserved.
  Synthesis uses 22050 Hz in emulated time, including PSG tone/noise/envelope
  clocks. Like the ordinary MSX firmware, PWM playback stays at a fixed
  22050 Hz; it never lowers pitch to hide a slow guest. Approximately three
  frames are buffered for scheduler jitter. A guest that cannot generate
  samples in real time reports underruns rather than silently retuning music.
  F12/mute clears buffered sound immediately.
  **PSG only + diagnostics** disables FM/PCM/SCC/click, enables PSG/master
  and enables UART diagnostics. **Play PWM test tone** deliberately bypasses
  master/source switches and the emulated chips for a two-second sine test;
  it always runs at 1x gain, returns to a quiet F12 menu and restores your
  configured gain and source choices.
- **Built-in firmware:** controls the original firmware switch; cold-boot
  to apply it. Off is the default for normal BASIC/DOS startup.
- **Auto boot** and **Present every N frames:** independent host settings.
  Presentation skipping does not accelerate the guest CPU or skip VDP
  state/sprite evaluation.
- **VGA fit:** Left/Right or Enter selects 80%, 85%, 90%, 95% or 100%.
  The default 90% centers the entire picture and every menu inside a safe
  area, keeping the 4:3 aspect ratio on monitors that clip the edges.
  100% restores the full 320x240 area. VGA timings, pixel clock and line
  doubling remain identical to normal MSX.
- **Save boot configuration:** saves both ROM paths, mapper choices, both
  disk paths, VGA fit and other preferences in its own NVS namespace,
  separate from ordinary MSX. Existing A1ST settings migrate automatically,
  retaining Drive A, sound and boot preferences; new slots start empty.
- **Media > Reboot and save configuration** and **Reboot without saving**:
  apply cartridge changes immediately, with or without persisting selections.
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

Cartridges use physical primary slots 1 and 2, as on the A1ST. ROMs must
contain an `AB` cartridge header and use 8 KiB multiples, from 8 KiB to 4 MiB.
Cartridge storage uses separate 8 KiB PSRAM banks rather than requiring one
contiguous allocation. F12 cartridge inspection streams through a 4 KiB
internal buffer and does not allocate another ROM image, even while a large
cartridge is running. On cold boot, the full selected cartridge is loaded
into PSRAM; guest bank switches and reads never fetch cartridge data from SD.
Allocation failures report the requested/allocated ROM bytes and remaining
PSRAM, including its largest free block, while retaining the previous slot.
Available PSRAM still limits the combined size of both
slots; two 4 MiB cartridges cannot fit alongside the emulated machine.
They can be selected from any folder on the card, including the normal
MSX firmware's ROM folders; files are not copied or modified.
The mapper choices match normal MSX: **Auto**, **Generic 8 KiB**,
**Generic 16 KiB**, **Konami SCC**, **Konami**, **ASCII8**, **ASCII16**,
**GameMaster2**, **FM-PAC** and **ASCII16-X**. Auto recognizes the `ASCII16X`
marker at ROM offset `0x10`, including the 4 MiB Illusion City cartridge
conversion. Keep this image intact and use **Auto** or **ASCII16-X**, not
ordinary **ASCII16**. ASCII16-X supports its extended bank registers and
mirrored memory windows; unpopulated flash reads as `0xFF`. Flash programming,
erasing and flash-backed saves are not emulated, and the ROM file is never
modified. Auto also handles plain/mirrored ROMs and uses
bank-write heuristics for common MegaROMs; it does not read fMSX mapper
databases. Use a manual override for ambiguous ROMs and GameMaster2/FM-PAC
images not recognized automatically. GameMaster2 requires 128 KiB and
FM-PAC requires 64 KiB. FM-PAC register writes use the built-in YM2413.
Special mappers beyond this list are not supported.

GameMaster2 and FM-PAC SRAM is saved beside the cartridge as
`<filename>.slot1.sram` or `<filename>.slot2.sram`, independently of the
normal MSX firmware's save files. Enter F12 before powering off to flush it.

**Tape remains unsupported in this A1ST firmware.** The real turboR removed
the cassette interface, and A1ST BASIC 4.0 does not expose the normal MSX
`CAS:` device. Use the normal MSX firmware for CAS tapes. No virtual tape
BIOS patches are installed.

Entering F12 also flushes firmware SRAM and CMOS. These are separate
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

## USB joysticks

Both joystick connectors now accept direct low-speed USB HID controllers:
D-/D+ on GPIO15/16 for MSX port 1 and GPIO17/18 for MSX port 2.
Open **F12 > USB joysticks / calibration**, choose a port and press Enter.
Capture neutral, up, right, down, left, button A and button B. Calibration
is saved immediately in the independent `a1st-joystick` NVS namespace;
Delete clears it. Uncalibrated controllers remain neutral. Normal MSX
calibration is not overwritten or imported. Hubs and full-speed pads
are not supported; Macintosh Plus continues to use its mouse.

The host uses CP400-compatible raw report capture and HID idle setup,
without requiring a readable or short HID report descriptor. Reports
remain limited to eight bytes, including any report ID. From `software`,
run `.\Test-UsbJoysticks.ps1` to exercise enumeration and report delivery
for every joystick firmware. `make test` also covers calibration, runtime
reconnection and packet timing.

## Implementation scope and limitations

Included: 256 KiB mapped RAM, 16 KiB firmware SRAM, 128 KiB V9958 VRAM,
separate Z80/R800 register banks, S1990 CPU/DRAM switching, Panasonic ROM
banking and DRAM aliases, Kanji ROM, PPI keyboard, PSG, MSX-MUSIC, RTC,
non-inverted reset status, E6/E7 timer, PCM output, pause/LED registers and
the original banked TC8566AF disk interface.
The Z80/R800 CPU state uses internal SRAM when it fits while preserving a
64 KiB reserve, matching the normal MSX CPU-memory policy; UART reports its
actual location and any PSRAM fallback. Inactive FM operators and silent DAC
channels are skipped instead of repeatedly synthesizing/mixing silent blocks.
VGA scaling uses exact rational stepping instead of dividing for every output
pixel. PSG square-wave averaging counts the same 16 phase samples analytically
when at most one half-cycle boundary is crossed, with the original loop retained
for larger steps. Silent/noise-gated channels still advance phase correctly.
These reduce CPU work without changing VGA pixels, VDP/sprite behavior or PSG
sample values.

Not claimed: A1GT/MIDI, cassette selection,
microphone recording, printer output, save states, reverse
execution, cycle-perfect R800/VDP timing or complete application
compatibility. The upstream CPU timing model is retained; openMSX is a
reference, not the linked desktop CPU core. PCM input reads a neutral
signal because this board has no microphone path. Mechanical disk noise
is omitted.

VGA reuses the existing 320x240, line-doubled driver, with a saved centered
safe-area setting for menus and emulation. High-resolution and
80-column output are downsampled; interlaced fields are not woven into a
480-line image. UART reports **emulated speed percentage**, presented FPS,
active CPU/DRAM mode and free PSRAM every five active seconds. Menu time
is excluded. Audio diagnostics also show the playback sample rate, queued
samples and underrun count since enabling/resuming sound. Persistent underruns
indicate that output still cannot keep up with frame timing.
With **Sound > UART audio diagnostics** enabled, an extra `A1ST audio debug:`
line every five active seconds reports the source mask, input sample count,
minimum/maximum, mean/DC level, RMS, clipping, played samples, late ISR ticks
(over twice the expected interval), largest ISR gap, PSG tone/noise/volume
registers, PCM status and keyboard-click edge count. Level/timing windows reset
each report; underruns reset on resume and click edges are cumulative since cold
boot. `gain` reports pre-quantization amplification; `pwmclip` counts output-gain
saturation separately from input `clip`. Intentional timer sleep is not an
ISR service gap. Logs are never printed
inside the audio interrupt, and detailed measurement is off by default.
Measure these on the board before drawing performance
conclusions.

To isolate persistent noise:

1. In F12 > Sound, run **Play PWM test tone**. This tests the output without
   any emulated sound source; its UART summary starts `A1ST audio test:`.
2. Select **PSG only + diagnostics**, resume the same MSX1 game and collect
   two or three `A1ST speed:` / `A1ST audio debug:` pairs.
3. Compare with **Enable all sound sources** and, separately, master sound
   Off. A clean reference tone but noisy game points toward synthesis/mixing
   or signal level; a noisy reference tone also implicates output timing/PWM.
   These controls are diagnostic tools, not a claim that the remaining
   board-reported noise has already been fixed.

## Validation

```powershell
make test
.\tools\Test-Core.ps1 `
    -Firmware 'C:\path\to\fs-a1st_firmware.rom' `
    -Kanji 'C:\path\to\fs-a1st_kanjifont.rom'
# Optionally add -Cartridge 'C:\path\to\ASCII16X.rom' to verify your image.
# Or add -Game 'C:\path\to\KMARE.ROM' for a 1800-frame audio smoke test.
.\tools\Test-Audio.ps1
.\tools\Test-SoundMenu.ps1
.\tools\Test-RomImport.ps1 -PanasonicDirectory 'C:\path\to\panasonic'
```

Host tests use native GCC/G++. They cover ROM/DRAM/SRAM mapping, S1990,
R800 multiplication, E6 timer phase, keyboard matrix, original TC8566AF
sector reads/writes, persistence, framebuffer bounds, VGA geometry,
settings validation, ROM importing and the shared FLH updater.
Audio regressions measure PSG pitch at 22050/44100 Hz and FM pitch/clipping.
PWM tests execute the real output code with a simulated hardware timer,
including frame jitter, timer rollover, silence, mute and large blocks. They
require fixed native-rate playback with no pitch modulation or underruns when
production meets the native rate. Deliberately insufficient 65%/50% supply must
report starvation and must not slow the output clock. Optional game tests read your ROM in
place, press Space/fire after 600 frames, and report only sample/clipping metrics;
they never record or distribute game audio.
Source-isolation tests verify independent PSG/FM/PCM/SCC and keyboard-click
muting, unchanged guest registers, old-settings migration and the actual
F12 sound page. Output tests validate input-level/clipping counters, late-ISR
timing detection, reporting-window reset and the chip-independent tone test.
Low-level waveform tests measure PWM quantization SNR through the actual output
code: a roughly 362-RMS sine improves from about 14.6 dB at 1x to 32.1 dB at 8x.
They also check saturating gain, output-clipping counts, reference-tone gain
bypass, gain persistence/migration and wakeup timing after intentional silence.
FM tests verify that 1000 inactive blocks use the null/silent fast path, key-on
resumes rendering immediately and reset clears the filter before returning silent.
Fast PSG math is checked against the original 16-step loop for 200000 random
cases and every valid tone period at phase boundaries. Fast VGA sampling is
compared pixel-for-pixel against the original formula at every supported fit
and both source widths.
Media regressions cover both external cartridge slots, all mapper choices,
SCC wave RAM, SRAM persistence, 4 MiB ASCII16/ASCII16-X banks, mirrored
ASCII16-X registers/windows, unpopulated flash, invalid-size/header rejection,
fragmented reads across separately allocated banks, both disk drives and failed-attachment
preservation. They also verify inspection with no extra heap allowance,
partial-allocation cleanup, and unchanged read-only shared firmware banks.
Frontend tests cover old-settings migration, full media
settings round trips, and centered, bounded 4:3 scaling at every VGA fit.
Browser navigation tests verify that 24 key events within a 142-entry
directory require only one directory scan, while page/folder changes,
wraparound, filters, eject, cancellation, errors and reopening still work.
Original-ROM tests also verify both cartridge initializers and Disk BASIC
directory listings from both drives.
The optional `-Cartridge` argument also checks ASCII16-X Auto/manual detection
and compares every byte in every populated bank and mirrored window against
the supplied file, read-only. This checks mapping, not game playability.
These original-ROM/cartridge tests enforce a 7.5 MiB limit on tracked emulator
C allocations, reserving 512 KiB of the board's 8 MiB budget for frontend and
other allocations, and repeat cold boot to check bank cleanup. Host accounting
does not simulate ESP-IDF heap overhead or fragmentation; on-board validation
is still required.
ROM-loading regressions also cover exact dump sizes, fragmented/interrupted
reads, unexpected EOF and I/O errors with a correct advertised size.
ROM-backed boot tests read your originals in place and keep generated
state only under the ignored `tests\build` directory.
