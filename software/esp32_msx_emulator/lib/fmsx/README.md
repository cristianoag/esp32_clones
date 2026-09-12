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
The portable governor measures execution time excluding pacing sleeps.
Every 100 ms of emulated time (six NTSC frames or five PAL frames), sustained
work above 105% of budget reduces upstream `UPeriod` proportionally, with
headroom, instead of slowly stepping down through multiple half-second windows.
The half-second window still handles small adjustments and restores five
percentage points when there is ample headroom (10–100% rendering).
This skips only rendering/presentation; it cannot make an overloaded CPU or
audio path run at real-time speed. It starts at 100% on a new boot or video
standard, so light workloads retain all display frames.

A keyboard callback blocked for at least 100 ms (the firmware F12 menu) resets
pacing and measurement history on return, avoiding menu-time catch-up bursts,
but retains the learned drawing percentage to avoid another slow ramp-up.
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
| Panasonic FS-A1F / MSX2 | `PANASONIC.ROM` (212992 bytes) |
| Panasonic FS-A1FX / MSX2+ | `PANASONIC.ROM` (212992 bytes) |
| Panasonic FS-A1WSX / MSX2+ | `PANASONIC.ROM` (344064 bytes) |

### Panasonic BIOS profiles

On MSX2/MSX2+, `PANASONIC.ROM` is an authoritative, headerless concatenation
of the user's original ROMs (no firmware modification or extracted sidecars):

| File offset | Size | Mapping |
| --- | --- | --- |
| `0x00000` | 32 KiB | BASIC/BIOS, primary slot 0, `0x0000–0x7FFF` |
| `0x08000` | 16 KiB | Sub-ROM, slot 3/subslot 1, `0x0000–0x3FFF` |
| `0x0C000` | 32 KiB | Kanji BASIC/driver, slot 3/subslot 1, `0x4000–0xBFFF` |
| `0x14000` | 128 or 256 KiB | Kanji font through I/O, not a memory slot |

The exact accepted totals are `0x34000` (FS-A1F/FS-A1FX) and `0x54000`
(FS-A1WSX). MSX2 accepts only `0x34000`; the larger WSX image requires MSX2+.
Open/read/allocation failures and wrong sizes stop boot rather
than falling back to split images. A directory containing both Panasonic and
Omega combined images is rejected. MSX1 ignores Panasonic images.
The sub-ROM and Kanji BASIC are one contiguous 48 KiB read-only allocation;
`DISK.ROM` cannot overwrite this window. The combined font takes precedence
over a separate `KANJI.ROM`. Same-model reset retains these allocations and
resets the font address; switching models releases/reloads the relevant ROMs.

These are **64 KiB generic fMSX BIOS/BASIC profiles**, not complete emulations
of each Panasonic motherboard. RAM remains in fMSX slot 3/subslot 2; original
BIOS code discovers it. Physical cartridge slots 1 and 2 remain available.
Original MSX2+ Kanji BASIC executes its own startup animation. No substitute
logo, forced boot patch, built-in application, model-specific disk controller,
firmware mapper, turbo control, FM-BASIC ROM or model-specific SRAM is added.
The optional generic Disk BASIC ROM and read-only cassette traps are described below.

Font level 1 uses D8h (column/address bits 5–10), D9h (row/bits 11–16),
and D9h reads. FS-A1WSX additionally uses DAh/DBh for level 2. Its address
and five-bit byte counter are shared by both levels: either address write
resets the counter, valid reads wrap within 32 bytes, and the read level must
match the most recent address-write level. A mismatched read returns FFh
without advancing the counter. FS-A1F/FS-A1FX have only D8h/D9h; DAh/DBh
are unmapped. This follows the hardware-tested openMSX configurations and
shared-latch/interlocked behavior in
[MSXKanji.cc](https://github.com/openMSX/openMSX/blob/master/src/MSXKanji.cc),
[FS-A1WSX](https://github.com/openMSX/openMSX/blob/master/share/machines/Panasonic_FS-A1WSX.xml),
[FS-A1F](https://github.com/openMSX/openMSX/blob/master/share/machines/Panasonic_FS-A1F.xml),
and [FS-A1FX](https://github.com/openMSX/openMSX/blob/master/share/machines/Panasonic_FS-A1FX.xml).

### Omega combined bank

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
Sprite collision events are prepared per visible line and consumed up to the
emulated horizontal beam position on status reads. A cleared collision cannot
reassert until the beam reaches another overlapping pixel; elapsed pixels are
never replayed. Magnification, clipping, sprite limits and mode-2 IC/color
controls participate in collision detection even when rendering is skipped.
This is a beam-timed collision approximation, not a cycle-exact VDP pipeline.
SCREEN 6 rendering applies the coarse/fine horizontal scroll registers, optional
two-page wrap and left-edge mask; its downsampled sprites select the proper
two-bit palette pair rather than producing a spurious light-blue bar.
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
CMOS writes stay in that same directory. Printer files and MIDI logging are
disabled by this frontend. Raw disk images are write-through; tape remains read-only.
Both joystick ports are configured as digital
MSX joysticks and receive the platform's active-high U/D/L/R/A/B masks through
`msxPollJoysticks` (bits 0-5 and 8-13). The core converts these to active-low
PSG input, including the register-15 port selector. Mouse input is not connected.
MSX hardware profiles are generic fMSX models: loading Omega firmware does
not emulate every physical Omega expansion or board peripheral.

### Boot-time cartridges

```cpp
bool MsxCoreRun(const char* romDirectory, int model, int ramPages,
                const char* slot1 = nullptr, const char* slot2 = nullptr,
                const char* diskA = nullptr, const char* diskB = nullptr,
                const char* tape = nullptr);
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

### Writable disks and read-only cassette

The last three boot arguments select drive A, drive B and one cassette. They
are required attachments when nonempty: a failed open/read/allocation aborts
before executing the Z80. `MsxValidateDisk` checks read/write access without
changing contents; `MsxValidateTape` checks read-only access. Both use the same
error-buffer convention as cartridge validation.
Live `MsxAttachDisk(unsigned drive, const char* path, char* error, size_t size)`,
`MsxAttachTape(path, error, size)` and `MsxRewindTape(error, size)` must run on
the CPU task while paused (the F12 callback). They return errors without
calling the platform's fatal error callback. Null/empty paths eject; failed
replacements preserve the old image/stream and cassette position. Disk swaps
commit only after a complete read and then reset pending FDC transfers.
The UI owns saved boot defaults separately from these live attachments.

Supported DSK images are **raw, headerless 368640 or 737280 bytes**, interpreted
as 80 tracks × 9 sectors × 512 bytes with one or two sides respectively. Data
is ordered track, side, sector. A 360 KiB 40-track/double-sided image is not
the supported geometry. Container/compressed images, other sizes and raw
controller/protection formats are unsupported. Sector contents, including an
`AB` prefix, do not invalidate a raw image. Disk storage and its small FDI
index are allocated in PSRAM, not the reserved internal heap.

Each selected BIOS profile may contain an optional **16 KiB `DISK.ROM`**.
It must be a compatible MSX Disk BASIC ROM with an `AB` header and standard
JP entries at 4010h/4013h/4016h/401Ch/401Fh. The verified example is the user's
Philips NMS8250 `nms8250_disk.rom`, SHA-1
`c3efedda7ab947a06d9345f7b8261076fa7ceeef`. Import it unchanged as `DISK.ROM`;
it is not distributed here. All models load it independently at **primary
slot 3/subslot 3/page 1 (4000h–7FFFh)**, leaving the complete Omega/Panasonic
extension at 3-1 and RAM at 3-2 untouched. It takes precedence over optional
RS232 ROM occupancy. `MsxDiskAvailable()` reports whether it was loaded and
patched. Missing/invalid disk BIOS does not prevent normal BASIC or cassette
use; attempting a disk attachment reports that a compatible `DISK.ROM` must
be added to that profile followed by a cold boot.

Disk accesses use original Disk BASIC executing fMSX BIOS traps, **not
TC8566 emulation or a claim of raw controller accuracy**. Traps are accepted
only from the mapped system ROM, not the same address in a cartridge.
Disk BIOS PHYDIO writes and WD1793 sector writes share `WriteFDI`: validate
the sector/file size, write the raw 512-byte sector, flush and `fsync`, then
update the PSRAM cache. Files stay open in `r+b` mode until ejected.
Unwritable files and duplicate A/B paths (case-insensitive) are rejected.
The WD1793 buffers an entire sector before committing it, so aborted or
incomplete controller transfers never reach the cache or backing file.
Long ESP32 writes periodically yield to the idle watchdog.

Host write/flush/sync errors become Disk BASIC write-fault code 10 or WD1793
`F_WRFAULT`, with a UART diagnostic; subsequent writes fail until reattachment.
Failed host writes may already have changed part or all of a physical sector,
so an error is not an atomic rollback guarantee. Save defaults and ejection
are not needed to persist successful writes. `SaveFDI` cannot export/truncate
an actively backed image. Disk formatting/WD1793 write-track operations,
saved-state auto-loading and empty-image creation remain unsupported.

CAS images must begin with `1F A6 DE BA CC 13 7D 74`; subsequent markers may
be byte-aligned anywhere. Tape reads use the original BIOS entry points.
The stream stays open read-only across F12 pauses; TAPION searches forward,
EOF returns carry/error and remains at EOF until explicit rewind or reattach.
TAPOON/TAPOUT/TAPOOF reject save attempts without changing the stream or source.
WAV and tape recording are unsupported. Disk contents are cached in memory
with an open write-through SD handle, whereas tape remains a read-only SD
stream: after physical SD removal/reinsertion, restart rather than trusting
old open handles.

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
  flash bank; load Panasonic BASIC/sub-ROM/Kanji BASIC/font combined images;
  implement WSX shared-latch, interlocked JIS level-2 font reads; implement the
  F4 reset-status latch and raster collision polling; independently map and
  patch Disk BASIC in slot 3-3; transactionally attach writable raw disks and read-only CAS.
- `Patch.c`: enforce mapped-ROM traps, write-through disks and read-only tape operations,
  bound disk sectors to mounted geometry, validate/fallback malformed BPBs,
  and support unaligned CAS markers with explicit EOF/rewind semantics.
- `FDIDisk.c`, `WD1793.c`: allocate new disk buffers through PSRAM, synchronize
  backed sector writes, buffer controller sectors and propagate host storage faults.
- `V9938.c`: add command-engine reset for safe profile switching.
- `Common.h`: apply SCREEN 6 coarse/fine horizontal scroll, two-page wrapping,
  left-edge masking and correct sprite palette-pair sampling.
- `Sound.c`: include embedded audio-driver declarations.
- `Floppy.c`: include POSIX directory declarations without selecting a desktop
  backend.
- `Esp32Port.h`, `library.json`, and `src/MsxCore.*`: new integration code.

All other vendored core files, including the Z80 interpreter and emulated sound
chips, remain original upstream sources.

## Host regression

`powershell -File lib\fmsx\tests\media.ps1` selects the synthetic media suite.
It executes original Z80 CALLs through patched BIOS entries and also checks
disk/tape register/error semantics, two drives, invalid/missing images,
allocation-failure rollback, eject/reattach, CAS rewind/EOF, controller/BIOS
write-through persistence, interrupted sector transfers, short-write/flush/sync
faults, duplicate mounts, slot isolation and unchanged CAS contents.
With local BIOS profiles, additionally run:

```powershell
powershell -File lib\fmsx\tests\media.ps1 -ProfilesRoot .\sdcard\msx\bios -DiskBios C:\private\nms8250_disk.rom -Frames 1200
```

Only disposable copies under `tests\.build` are used. This boots all six
profiles with original BIOS code, lists synthetic files using `FILES "A:"`
and `FILES "B:"`, executes `BLOAD "CAS:"`, and verifies the loaded byte in BASIC.
It also saves distinct BASIC programs to A and B, verifies their directory
entries in the backing files, cold-boots again, and loads/runs both saved
programs to prove persistence beyond the original emulator's RAM cache.
All six profiles passed this test with the identified Philips ROM. No BIOS
or game bytes are embedded in tests, and originals/prepared profiles are not
modified. The optional real-media verification requires at least 900 frames
per boot; the command above uses 1200.

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
Panasonic tests cover the model-specific combined sizes (including rejection
of a WSX-sized image on MSX2), immutable
48 KiB extension mapping, all cartridge-slot selections, font contents and
maximum addresses, 32-byte wrapping, shared address/counter and mismatched-level
reads, absent level-2 ports, model/reset cleanup, ambiguous combined banks,
unreadable/short files, allocation failures and successful legacy recovery.
Use `-PanasonicOnly` to select just the Panasonic ROM/slot/font regressions
(plus the portable frame-pacing tests); the default still runs the complete
synthetic lifecycle, cartridge and video suites. This selector can be combined
with either real-BIOS directory option below.
Beam-timed sprite collision tests are separate in `tests\animation.ps1`;
the lifecycle suite does not assume collisions can reassert without beam progress.
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

The script copies `PANASONIC.ROM` alone for Panasonic MSX2/MSX2+ or `OMEGA.ROM`
alone when present for MSX2+, otherwise the model's
generic BIOS files and optional MSX2+ logo, into its ignored build
directory (the originals are never modified), runs 900 unpaced emulated frames,
prints the final text-mode character table and writes `tests\.build\boot.ppm`.
It also saves frame 30 (or the final frame for shorter runs) to `boot-early.ppm`.
For a non-erased logo image, it requires samples of the real CPU executing
0x8000–0xBFFF with slot 0 mapped, and captures the most detailed observed frame
during that execution as `boot-logo.ppm`. These checks supplement, rather than
replace, visual inspection of the capture. For a Panasonic MSX2+ or Omega image containing
Kanji BASIC, the test instead requires a SCREEN 6 logo frame with white
lettering before BASIC. An erased auxiliary window is not mistaken for
absence of the built-in MSX2+ startup logo.
A successful real-BIOS test requires a detected BASIC `Ok` prompt. An absent
prompt fails the test but may indicate a graphical boot screen rather than
broken emulation; inspect the captured image and transcript. Private BIOS
copies are removed in a `finally` block. To test all six imported profiles
with one compilation, use `-ProfilesRoot .\sdcard\msx\bios` instead of
`-BiosDirectory`. Captures are also saved as `boot-expert.ppm`,
`boot-hotbit.ppm`, `boot-omega.ppm`, `boot-fs-a1f.ppm`,
`boot-fs-a1fx.ppm`, and `boot-fs-a1wsx.ppm`.
Early/logo captures receive the corresponding profile suffix as well.

Real-BIOS host validation on September 7, 2026 reached a rendered BASIC `Ok`
prompt after 900 frames in each of the supplied profiles:

| Profile | Model / RAM | Reported BASIC |
| --- | --- | --- |
| Expert | MSX1 / 64 KiB | MSX BASIC 1.1 Br, Gradiente |
| Hotbit | MSX1 / 64 KiB | HOT-BASIC V1.2, EPCOM |
| Omega | MSX2+ / 512 KiB | MSX BASIC 3.0, Microsoft |

Additional native validation on September 10, 2026 reached BASIC `Ok` after
900 frames with all three user-supplied Panasonic combined images:

| Profile | Model / RAM | Reported BASIC |
| --- | --- | --- |
| FS-A1F | MSX2 / 64 KiB | MSX BASIC 2.0, 28815 bytes free |
| FS-A1FX | MSX2+ / 64 KiB | MSX BASIC 3.0, 28815 bytes free |
| FS-A1WSX | MSX2+ / 64 KiB | MSX BASIC 3.0, 28815 bytes free |

FX and WSX each produced a SCREEN 6 startup logo capture at frame 174 with
7471 white lettering pixels. FS-A1F booted without needing S1985-specific
mirrors or extra peripheral emulation. These are native-core results, not
proof of board timing, full motherboard emulation or every Kanji application.

### Focused startup-animation regression

Run `powershell -File lib\fmsx\tests\animation.ps1` from the firmware directory
for BIOS-free synthetic tests of all 512 SCREEN 6 coarse/fine scroll
combinations, two-page wrapping, masks, sprite palette pairs, beam-timed
collision reads, EI/HBlank timing, magnification, clipping, sprite limits,
IC/color handling and MSX1 compatibility.

For evidence from a local, user-supplied Omega image:

```powershell
powershell -File lib\fmsx\tests\animation.ps1 -BiosDirectory .\sdcard\msx\bios\omega -Capture -Raster
```

For FS-A1FX or FS-A1WSX, select that profile's directory and add `-RamPages 4`
(64 KiB). The animation harness tests MSX2+; use the lifecycle boot test for
FS-A1F/MSX2, which does not have the MSX2+ startup animation.

The isolated `tests\.build\animation` directory contains the measurements in
`animation.csv`, optional ordered `animation-*.ppm` captures and `animation.gif`,
and optional `raster.csv` CPU/VDP-write traces. The GIF is a preview capped at
30 Hz to avoid browser minimum-delay stretching and includes the final BASIC
screen; CSV/PPM evidence retains the full emulated 60 Hz timeline.
The script copies `OMEGA.ROM` or, when absent,
`PANASONIC.ROM` into its isolated BIOS directory and cleans that copy and CMOS
output afterward. `-RamPages` defaults to 32 (Omega's 512 KiB); `-Frames` defaults
to 900; `-DrawPercent 10` tests low presentation frequency without changing
emulated timing. `-Raster` enables debug tracing only for this native test.

Native Omega measurements with the corrected renderer/collision timing showed
54 distinct full-rate animation frames versus 10 in the baseline, with SCREEN 6
present for 177 emulated frames (about 2.95 seconds) and BASIC reached at
emulated frame 318. At 10% drawing, nine distinct presentations were observed
while BASIC still arrived at frame 318. These are emulated-frame measurements,
not ESP32 wall-clock performance claims; no additional pacing change was made
for this animation fix.

FS-A1FX and FS-A1WSX at 64 KiB each also produced 54 distinct animation frames,
177 emulated SCREEN 6 frames and BASIC at frame 318, with 7471 white lettering
pixels. The same capture/raster harness exercised the unmodified combined
images; FS-A1F's separate MSX2 boot check reached BASIC without requiring this
MSX2+ animation.

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
