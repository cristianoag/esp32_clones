# Local firmware dependencies

MSX builds and tests use only this firmware folder plus the installed
PlatformIO platform/toolchain. No build inputs are read from another
firmware project.

These sources were copied from the board-compatible versions previously
used by the CP400 firmware on 2026-09-12, without source changes:

| Dependency | Local copy | Version / origin |
| --- | --- | --- |
| Board definition | `../boards/esp32-s3-devkitc-1-n16r8.json` | Existing N16R8 board configuration |
| VGA | `ESP32-S3-VGA-main` | bitluni ESP32 S3 VGA 0.1.0, including the existing board adaptations |
| Graphics | `Adafruit-GFX-Library-master` | Adafruit GFX 1.12.1 |
| Peripheral I/O | `Adafruit_BusIO-master` | Adafruit BusIO 1.17.1 |
| Low-speed USB | `MsxSoftUsb/upstream` | Existing Dmitry Samsonov/tobozo transport with the MSX adaptations |

The VGA/GFX/BusIO directories retain their tracked source, examples,
metadata and attribution/license files. USB's five required source/header
files retain their original attribution. The MSX adapter compiles that
transport once with `MSX_SOFT_USB`; the CP400 CPU ticker and wrapper are
not part of this firmware.

Copies are intentionally independent, not symlinks. A later dependency fix
in one firmware must be reviewed and applied separately to the other if
needed. Do not replace these copies with references to a sibling firmware.

The fMSX core was already local; its separate non-commercial terms and
modifications are documented in [fmsx/README.md](fmsx/README.md).
