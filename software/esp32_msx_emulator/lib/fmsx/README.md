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
256-color output. On ESP32 the core uses the monotonic ESP timer and yielding
FreeRTOS delays to pace PAL at 50 Hz and NTSC at 60 Hz, with `UPeriod=100`.
An overdue deadline is reset after a blocking firmware menu, avoiding catch-up
bursts. Every presented frame unconditionally yields at least one FreeRTOS tick
so the idle/task watchdog remains serviced even when emulation is slower than
the target rate. Late frames do not incur an additional full-frame sleep.
The platform's presentation callback must not add its own delay.
The platform owns VGA conversion, input and PCM output. Audio is signed
16-bit mono at 22050 Hz from upstream `Sound.c`. Host regressions are unpaced.

The BIOS directory must be an absolute path in the mounted POSIX filesystem.
Required files:

| Model | Required BIOS files |
| --- | --- |
| MSX1 | `MSX.ROM` (32768 bytes) |
| MSX2 | `MSX2.ROM` (32768), `MSX2EXT.ROM` (16384) |
| MSX2+ | `MSX2P.ROM` (32768), `MSX2PEXT.ROM` (16384) |

Upstream optional files, including `DISK.ROM`, `CMOS.ROM`, `KANJI.ROM`, and
system cartridges, are resolved inside the selected profile directory.
CMOS writes stay in that same directory. Disk auto-insertion, tape, printer
files and MIDI logging are disabled by this frontend.
The underlying disk/tape emulation is retained, but this frontend does not
offer an image-selection API. Joystick/mouse host input is not connected.
MSX hardware profiles are generic fMSX models: loading Omega firmware does
not emulate every physical Omega expansion, logo-ROM slot or board peripheral.

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
  changes; allocate tracked RAM/VRAM/ROM/font buffers in PSRAM; guard failed
  allocations before forming hardware pointers; accept 64 KiB RAM on later
  models; reset frame/blink counters and pending VDP operations between boots;
  require explicit cartridge loads, reject incomplete/oversized ROMs, make SRAM
  allocation failures fatal, reset mapper registers and disable cartridge
  automatic state restoration.
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
polling, exit, seven PSRAM-allocation failure points and subsequent recovery,
and missing-BIOS failure. A second synthetic BIOS scans both physical slots,
calls each synthetic cartridge's Z80 entry point and verifies its result in RAM.
The cartridge regressions cover each slot separately and together, all five
plain sizes, all eight original mappers (including heuristic and database
selection), actual bank writes/reads, 2 MiB images in both slots, complete loaded
contents, SCC reset, eject/switch, malformed/missing files, every required
cartridge/SRAM allocation failure and successful recovery without any CPU
execution on failed boots. The host test replaces only ESP32 allocation and
platform I/O, not any emulated component. Build artifacts and generated test
ROMs are confined to the ignored `tests\.build` directory.

Passing this regression does not establish physical VGA/audio/USB operation
or compatibility with a particular real BIOS; those require board testing.

To additionally boot real, user-supplied BIOS files, pass their native absolute
directory path to the same script, for example:

```powershell
powershell -File lib\fmsx\tests\host_smoke.ps1 -BiosDirectory C:\private\omega -Model 2 -RamPages 32 -Frames 900
```

The script copies only the model's required BIOS files into its ignored build
directory (the originals are never modified), runs 900 unpaced emulated frames,
prints the final text-mode character table and writes `tests\.build\boot.ppm`.
A successful real-BIOS test requires a detected BASIC `Ok` prompt. An absent
prompt fails the test but may indicate a graphical boot screen rather than
broken emulation; inspect the captured image and transcript. Private BIOS
copies are removed in a `finally` block. To test all three imported profiles
with one compilation, use `-ProfilesRoot .\sdcard\msx\bios` instead of
`-BiosDirectory`. Captures are also saved as `boot-expert.ppm`,
`boot-hotbit.ppm`, and `boot-omega.ppm`.

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
