# Embedded mapper database

`romdb.h` is a byte-for-byte copy of the mapper metadata and binary lookup
from Cristiano Goncalves / The Retro Hacker's MSX PicoVerse project:

- Repository: <https://github.com/cristianoag/msx-picoverse>
- Source: `2350/software/loadrom.pio/tool/src/romdb.h`
- Local source revision: `3034c5e754cf1450cfc8f8d09af8bc0aa28e8c8a`
- SHA-256: `3cd060ecf403f81bf5eb08430eff3e55eb9bdc88ac7f2c7686c42608184c93f9`
- Snapshot: 3115 sorted SHA-1 / mapper entries.

PicoVerse distributes its tool under CC BY-NC-SA 4.0; its license is retained
in `PicoVerse-LICENSE.txt`. The generated metadata comes from openMSX's
`softwaredb.xml`, with original database credits to Nicolas Beyaert,
the BlueMSX team and the openMSX team. No ROM/BIOS bytes, UF2 firmware, Pico
hardware drivers or openMSX emulator implementation are included here.

The local `EMULib/MapperDatabase.c` adapter uses fMSX's existing SHA-1
implementation over the complete cartridge and translates the PicoVerse
IDs to fMSX IDs; these numbering schemes are not interchangeable.
Supported entries cover Konami SCC, Konami, ASCII8 and ASCII16.
Known ASCII16-X/Manbow2 entries and NEO8/NEO16 signatures produce explicit
unsupported-mapper errors, not a guessed compatible device.

For banked cartridges, explicit profile-local `CARTS.CRC` / `CARTS.SHA`
overrides retain priority, then this embedded database/signature check runs,
then fMSX's existing heuristic handles unknown images. Plain/planar ROM
layouts and the emulator's 2 MiB cartridge limit are unchanged.
The fallback is deliberately the existing fMSX heuristic rather than
PicoVerse's broader hardware-specific fallbacks.

The table is constant data stored in firmware flash, not copied to PSRAM.
There is no runtime dependency on PicoVerse, openMSX, a network connection or
a database file on the microSD card. To refresh it, copy a new generated
PicoVerse header, update the provenance, and run the mapper/core regressions
before rebuilding the firmware.
