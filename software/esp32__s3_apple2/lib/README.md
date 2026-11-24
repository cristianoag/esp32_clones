# Dependencies and provenance

This firmware is self-contained. Its shared board dependencies and board
adapters were copied from the repository's TK firmware on 2026-09-29,
preserving original notices and licenses.

| Dependency | Local location | Origin/version |
| --- | --- | --- |
| ESP32-S3 N16R8 board | `../boards` | Shared Clone Series board definition |
| VGA | `ESP32-S3-VGA-main` | bitluni ESP32 S3 VGA 0.1.0, existing board adaptations |
| Graphics and ASCII font | `Adafruit-GFX-Library-master` | Adafruit GFX 1.12.1 |
| Bus I/O | `Adafruit_BusIO-master` | Adafruit BusIO 1.17.1 |
| Software USB | `MsxSoftUsb` | Existing Dmitry Samsonov/tobozo transport and board adapter |
| NMOS 6502 | `chips/m6502.h` | floooh/chips `9e88298ce56319953ac7a43213a1120359f7a3a6`, zlib license |

The `Apple*` board adapters retain their original non-commercial notices.
They use Apple-specific NVS and diagnostics; OTA accepts compatible
FLH packages from all four emulators. The USB
transport library retains its upstream-facing `MsxSoftUsb` API. Keyboard
input adds an ASCII queue/repeat path alongside the inherited raw-report
transport. Audio, joystick and firmware-package regression tests are
local copies of the corresponding TK tests, adapted for Apple.

The initial reference is
[codesafe/ESP32-VGA_AppleII_Emulator](https://github.com/codesafe/ESP32-VGA_AppleII_Emulator),
revision `a47387e093a76a0132738bf9518c8996c685c4d2`.
Its disk-stepper algorithm was adapted for bounded, read-only media.
The project owner confirmed permission to reuse that upstream source
during this integration. Upstream declares no general license; this
permission must not be interpreted as a blanket third-party relicensing.
Its embedded ROMs, font bitmap and game data are not included.

IIe memory-switch behavior was additionally informed by
[ArthurFerreira2/reinetteIIe](https://github.com/ArthurFerreira2/reinetteIIe),
revision `ae42248f38d8615db6d7b9ae0f7e71678cd932bd`.
Its MIT notice is preserved in [Reinette-LICENSE](Reinette-LICENSE).
Its SDL UI, 65C02 core, ROMs and disk images are not part of this firmware.
The IIe banking and platform-independent machine implementation live in
`../src`, not in an imported desktop emulator.

The chips CPU retains its original execution logic, with the full notice
in its header and [license](chips/LICENSE). The only local header change is
an optional `M6502_TICK_ATTR` annotation on the tick declaration/definition.
It defaults to empty for host tests; the ESP32 build uses it to place the
hot decoder in internal instruction RAM rather than flash.
Third-party licenses are not replaced by the
repository license. ROMs used for local smoke tests are external inputs,
not firmware or test fixtures distributed here.
