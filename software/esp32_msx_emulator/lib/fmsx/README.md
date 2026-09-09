# Original fMSX 6.0 ESP32 port

This firmware uses **fMSX 6.0 by Marat Fayzullin**, not the retro-go/Svarkovsky
adaptation. Upstream: <https://fms.komkon.org/fMSX/>.
The selected sources came from the official `fMSX60.zip` distribution:
<https://fms.komkon.org/fMSX/fMSX60.zip>, retrieved September 7, 2026.
Original copyright notices are retained in every upstream source file.

## Licensing

fMSX is source-available under its author's noncommercial terms, **not MIT,
BSD, GPL, or another unrestricted open-source license**. The official site
permits non-profit use with proper credit to Marat Fayzullin and his URL.
Commercial use requires a separate license from the author:
<mailto:marat@komkon.org>. The source headers prohibit commercial distribution
and ask that the author be notified of modifications. This port does not claim
that such notification has already been sent.

See the upstream site for the authoritative terms. These terms apply to the
vendored fMSX, Z80, and EMULib code regardless of any repository-wide license.
No BIOS, game cartridge, or other third-party ROM binary is supplied.

## Integration

Build the C sources selected by `library.json` with `LSB_FIRST`, `FMSX`,
`BPP8`, `BPS16`, and `NARROW` defined. Add this directory and its `fMSX`,
`EMULib`, and `Z80` subdirectories to the include search path. Do not enable
`UNIX`, `DEBUG`, `ZLIB`, `EXECZ80`, or a desktop platform backend.
`src/MsxCore.cpp` supplies the embedded host and includes the upstream
`Common.h` renderer. The upstream desktop main, menus, and frontends are
not built.

The blocking `MsxCoreRun` API boots original Z80 BIOS code with the upstream
slot/memory-mapper, PPI, AY8910, SCC, YM2413, TMS9918/V9938/V9958, and VDP command
implementations. `Common.h` downscales 512-pixel graphics and 80-column text.
A 272x240 intermediate buffer preserves horizontal-adjust safety; its central
256x240 region is submitted using a fixed GGGRRRBB palette represented as
RGB888 values. Programmable colors and MSX2+ YJK colors are quantized to this
256-color output. On ESP32 an unconditional emulated-frame callback uses the
monotonic ESP timer to pace PAL at 50 Hz and NTSC at 60 Hz, independently of
presentation. Fractional NTSC deadlines avoid integer-period drift. Z80 clock
constants, scanlines, keyboard/joystick polling and sound generation are unchanged.
Every half-second of emulated frames, a portable governor measures execution
time excluding pacing sleeps. Under budget pressure it reduces upstream
`UPeriod` by 10 percentage points, and with ample headroom restores 5 points
(10–100% rendering). This skips only rendering/presentation; it cannot make an
overloaded CPU or audio path run at real-time speed. It starts at 100% each boot.

A keyboard callback blocked for at least 100 ms (the firmware F12 menu) resets
pacing and measurement history on return, avoiding menu-time catch-up bursts.
Short scheduler jitter is retained, but overdue deadlines never accumulate
more than one frame of catch-up debt. FreeRTOS delays service the idle/task
watchdog while ahead; when behind, a one-tick yield is requested at the next
frame boundary after 50 ms without a yield, rather than on every drawn frame.
Thus the worst interval includes one frame's execution time.
The platform's presentation callback must not add its own delay.
The platform owns VGA conversion, input and PCM output. Audio is signed
16-bit mono at 22050 Hz from upstream `Sound.c`. Every five active seconds UART
reports **emulated fps / target**, presented fps and current drawing percentage.
F12 waits are excluded by restarting the measurement window. Hardware speed
must be measured with these counters; host simulations are not ESP32 benchmarks.
Host regressions remain unpaced with adaptive rendering disabled.

CPU-addressed RAM, BIOS/extension ROMs, cartridges and cartridge SRAM of at
most 64 KiB preferentially use internal byte-addressable SRAM, only when at
least 64 KiB would remain free. Allocation failure/fragmentation falls back to
PSRAM and the chosen placement is logged at boot. Larger allocations (including
Omega's 512 KiB RAM) deliberately remain in PSRAM. Video buffers, VRAM, empty
slot storage and font buffers stay in PSRAM regardless of size; this is not an
attempt to fit entire frames or large machines into internal SRAM.

The BIOS directory must be an absolute path in the mounted POSIX filesystem.
Required files:

| Model | Required BIOS files |
| --- | --- |
| MSX1 | `MSX.ROM` (32768 bytes) |
| MSX2 | `MSX2.ROM` (32768), `MSX2EXT.ROM` (16384) |
| MSX2+ | `MSX2P.ROM` (32768), `MSX2PEXT.ROM` (16384) |
| Omega / MSX2+ | `OMEGA.ROM` (262144 bytes), instead of separate files |

For MSX2+, an existing `OMEGA.ROM` takes precedence over the generic files,
regardless of the profile directory's name. This is **one selected 256 KiB flash
bank**, containing the main BIOS, logo area and sub-ROM together. The loader
reads only the mapped regions directly into their CPU buffers:

| File offset | Size | Mapping |
| --- | --- | --- |
| `0x00000` | 32 KiB | Primary slot 0, `0x0000–0x7FFF` |
| `0x08000` | 16 KiB | Auxiliary primary-slot-0 window, `0x8000–0xBFFF` |
| `0x10000` | 16 KiB | Slot 3/subslot 1, `0x0000–0x3FFF` |
| `0x14000` | 32 KiB | Kanji BASIC/startup, slot 3/subslot 1, `0x4000–0xBFFF` |

It neither extracts separate files nor allocates the entire 256 KiB container.
An invalid size, open/read failure or allocation failure stops boot; an existing
but invalid combined bank never falls back to legacy files. The bank's unused
regions and expansion ROMs do not automatically add emulated peripherals.
The combined image's extension and Kanji BASIC form one contiguous 48 KiB
allocation. A separate generic `DISK.ROM` is not overlaid on this system
window. The MSX2+ F4 register reports inverted cold/warm reset status.
Sprite collision detection accounts for magnification and the current
visible line, including collision reassertion after status reads during
an overlapping line. This remains scanline-level emulation, not a
cycle-exact VDP pixel pipeline.
MSX1/MSX2 ignore `OMEGA.ROM`. The original generic format remains supported
when the MSX2+ directory has no combined bank.

Generic MSX2+ additionally supports optional `MSX2PLOGO.ROM` (exactly 16384 bytes).
It maps read-only at **primary slot 0, 0x8000–0xBFFF**, matching Omega's flash
bank offset `0x08000`; it never occupies cartridge slots 1 or 2. The normal
BIOS decides whether/how to execute this ROM; the host does not draw a substitute
logo or patch the boot sequence. Missing files preserve the original boot.
An existing file with an invalid size, open/read failure or allocation failure
stops boot. Same-model resets preserve the mapping; changing models or ending
a run releases it. Other models ignore this file.

Upstream optional files, including `DISK.ROM`, `CMOS.ROM`, `KANJI.ROM`, and
system cartridges, are resolved inside the selected profile directory.
CMOS writes stay in that same directory. Disk auto-insertion, tape, printer
files and MIDI logging are disabled by this frontend.
The underlying disk/tape emulation is retained, but this frontend does not
offer an image-selection API. Both joystick ports are configured as digital
MSX joysticks and receive the platform's active-high U/D/L/R/A/B masks through
`msxPollJoysticks` (bits 0-5 and 8-13). The core converts these to active-low
PSG input, including the register-15 port selector. Mouse input is not connected.
MSX hardware profiles are generic fMSX models: loading Omega firmware does
not emulate every physical Omega expansion or board peripheral.

### Boot-time cartridges

```cpp
bool MsxCoreRun(const char* romDirectory, int model, int ramPages,
                const char* slot1 = nullptr, const char* slot2 = nullptr);
bool MsxValidateCartridge(const char* absolutePath, char* error, size_t errorSize);
```

The two optional absolute POSIX paths (for example `/sdcard/msx/roms/demo.rom`)
load through upstream `ROMName`/`LoadCart` into **physical primary slots 1 and
2**, respectively. Null or empty paths eject that slot on the next boot.
The paths must remain valid until this blocking call returns. There is no hot
insertion: both requested cartridges must load completely before the Z80 runs.
Missing/nonregular files, invalid sizes or AB headers, read failures and required
ROM/SRAM allocation failures stop boot with a platform error, rather than
silently falling back to BASIC.
The read-only `MsxValidateCartridge` helper checks size, readability and AB header
without altering live emulation, so menus can preflight files before saving a
selection. Null/empty is valid; its optional error buffer is cleared on success
and receives a bounded NUL-terminated explanation on failure. Boot validates
again and the loader still requires a complete read, since a file can change
after preflight.

Images must contain complete 8 KiB banks and be at most **2 MiB per cartridge**
(`MSX_MAX_CART_BYTES`). This is the original core's 256-bank, byte-sized mapper
limit; larger images cannot be addressed correctly. Plain 8/16/32 KiB cartridges
and flat 48/64 KiB images are supported; flat images use their AB header at file
offset `0x4000`, as expected by upstream. MegaROMs use original fMSX mapper
autodetection: Generic 8/16 KiB, Konami4, Konami5/SCC, ASCII8, ASCII16,
GameMaster2 and FMPAC. Optional profile-local `CARTS.CRC` and `CARTS.SHA`
databases take precedence over instruction-pattern heuristics. Heuristics are
not a guarantee for every game, especially Generic16/GameMaster2/FMPAC, and
unsupported mapper hardware is not added by this frontend.

Each run resets cartridge mapping, mapper selection/registers and SCC state.
Cartridge `.STA` files are not automatically restored. Upstream SRAM-capable
mappers load adjacent `.sav` files and save modified SRAM during normal exit;
boot-time allocation of both SRAM and its filename is mandatory.

## Local modifications

- `MSX.c`: use explicit path resolution instead of process-wide directory
  changes; allocate tracked buffers through the embedded memory policy; prefer
  reserved-budget internal SRAM for small CPU-addressed RAM/ROM buffers; guard failed
  allocations before forming hardware pointers; accept 64 KiB RAM on later
  models; reset frame/blink counters and pending VDP operations between boots;
  require explicit cartridge loads, reject incomplete/oversized ROMs, make SRAM
  allocation failures fatal, reset mapper registers and disable cartridge
  automatic state restoration; call the platform once per emulated frame even
  when rendering is skipped; load the optional MSX2+ logo into slot 0 page 2,
  with exact-size/read checks and tracked, model-safe cleanup; directly load
  MAIN/auxiliary/SUB/Kanji BASIC regions from an authoritative single Omega
  flash bank; implement the F4 reset-status latch and raster collision polling.
- `V9938.c`: add command-engine reset for safe profile switching.
- `Sound.c`: include embedded audio-driver declarations.
- `Floppy.c`: include POSIX directory declarations without selecting a desktop
  backend.
- `Esp32Port.h`, `library.json`, and `src/MsxCore.*`: new integration code.

All other vendored core files, including the Z80 interpreter, emulated sound
chips, and software scanline renderer, remain original upstream sources.

## Host regression

With native GCC/G++ on PATH and the existing PlatformIO Arduino ESP32 SDK
installed, run `powershell -File lib\fmsx\tests\host_smoke.ps1` from the firmware
directory. This builds only the core and bridge, not the firmware. It uses a
small synthetic Z80 program (no BIOS content) to exercise all three models and
four RAM sizes twice, actual PSG tone generation, rendered pixels, keyboard
polling, both joystick ports through PSG register selection, exit, seven
PSRAM-allocation failure points and subsequent recovery,
and missing-BIOS failure. PAL/NTSC skip regressions verify that fewer presentations
do not suppress keyboard polling or sound generation. Portable synthetic-clock
tests cover exact PAL/NTSC pacing, overload and recovery, menu reset, mode changes,
coarse RTOS tick jitter, and an explicitly simulated rendering-cost workload.
They do not measure physical ESP32 throughput or internal-SRAM placement.
A second synthetic BIOS scans both physical slots,
calls each synthetic cartridge's Z80 entry point and verifies its result in RAM.
The cartridge regressions cover each slot separately and together, all five
plain sizes, all eight original mappers (including heuristic and database
selection), actual bank writes/reads, 2 MiB images in both slots, complete loaded
contents, SCC reset, eject/switch, malformed/missing files, every required
cartridge/SRAM allocation failure and successful recovery without any CPU
execution on failed boots. Logo regressions execute a synthetic Z80 program
from slot 0 page 2, read both 8 KiB halves, reject writes, preserve both cartridge
slots, exercise same-model resets and model changes, and cover presence/absence,
malformed/unreadable files and every boot allocation failure/recovery.
Combined-bank tests also verify authoritative precedence over malformed legacy
files, operation with no split files, correct region offsets, absence of
extracted files, truncated/oversized banks (including an unselected 512 KiB
source), a short read after size validation, generic fallback only when the
bank is absent, and allocation-failure recovery.
The host test replaces only ESP32 allocation and
platform I/O, not any emulated component. Build artifacts and generated test
ROMs are confined to the ignored `tests\.build` directory.

Passing this regression does not establish physical VGA/audio/USB operation
or compatibility with a particular real BIOS; those require board testing.

To additionally boot real, user-supplied BIOS files, pass their native absolute
directory path to the same script, for example:

```powershell
powershell -File lib\fmsx\tests\host_smoke.ps1 -BiosDirectory C:\private\omega -Model 2 -RamPages 32 -Frames 900
```

The script copies `OMEGA.ROM` alone when present for MSX2+, otherwise the model's
generic BIOS files and optional MSX2+ logo, into its ignored build
directory (the originals are never modified), runs 900 unpaced emulated frames,
prints the final text-mode character table and writes `tests\.build\boot.ppm`.
It also saves frame 30 (or the final frame for shorter runs) to `boot-early.ppm`.
For a non-erased logo image, it requires samples of the real CPU executing
0x8000–0xBFFF with slot 0 mapped, and captures the most detailed observed frame
during that execution as `boot-logo.ppm`. These checks supplement, rather than
replace, visual inspection of the capture. For an Omega image containing
Kanji BASIC, the test instead requires a SCREEN 6 logo frame with white
lettering before BASIC. An erased auxiliary window is not mistaken for
absence of the built-in MSX2+ startup logo.
A successful real-BIOS test requires a detected BASIC `Ok` prompt. An absent
prompt fails the test but may indicate a graphical boot screen rather than
broken emulation; inspect the captured image and transcript. Private BIOS
copies are removed in a `finally` block. To test all three imported profiles
with one compilation, use `-ProfilesRoot .\sdcard\msx\bios` instead of
`-BiosDirectory`. Captures are also saved as `boot-expert.ppm`,
`boot-hotbit.ppm`, and `boot-omega.ppm`.
Early/logo captures receive the corresponding profile suffix as well.

Real-BIOS host validation on September 7, 2026 reached a rendered BASIC `Ok`
prompt after 900 frames in each of the supplied profiles:

| Profile | Model / RAM | Reported BASIC |
| --- | --- | --- |
| Expert | MSX1 / 64 KiB | MSX BASIC 1.1 Br, Gradiente |
| Hotbit | MSX1 / 64 KiB | HOT-BASIC V1.2, EPCOM |
| Omega | MSX2+ / 512 KiB | MSX BASIC 3.0, Microsoft |

The Omega test used only the 32 KiB main BIOS and 16 KiB sub-ROM, without the
physical logo ROM or disk ROM. This confirms the generic fMSX BIOS boot path,
not full Omega board emulation or real-device operation.

September 8 validation of the single supplied Omega bank captured the
original MSX logo, including its 512 KiB RAM caption, and then reached BASIC.
The user also confirmed the logo and subsequent BASIC boot on the ESP32 board.
The auxiliary areas at `0x08000`/`0x48000` are erased; this ROM's startup
animation resides in Kanji BASIC at `0x14000` in the selected bank.
A populated, compatible logo ROM is still required to validate a real Omega
logo on either the host or hardware; an early-frame capture is not a replacement.
