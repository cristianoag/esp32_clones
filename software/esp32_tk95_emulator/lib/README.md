# Local dependencies and reference

This firmware is independent of the CP400/MSX project directories.
Local board dependencies were copied from the repository's MSX firmware
on 2026-09-26, preserving notices, licenses and board adaptations:

| Dependency | Local location | Origin/version |
| --- | --- | --- |
| ESP32-S3 N16R8 board | `../boards` | Shared Clone Series board definition |
| VGA | `ESP32-S3-VGA-main` | bitluni ESP32 S3 VGA 0.1.0 with existing board modifications |
| Graphics | `Adafruit-GFX-Library-master` | Adafruit GFX 1.12.1 |
| Bus I/O | `Adafruit_BusIO-master` | Adafruit BusIO 1.17.1 |
| Software USB | `MsxSoftUsb` | Existing Dmitry Samsonov/tobozo transport and MSX board adapter |
| Z80 | `chips/z80.h` | floooh/chips, revision `9e88298ce56319953ac7a43213a1120359f7a3a6`, zlib license |

The board support files in `../src` use `Tk*` filenames and identifiers,
including their test references and TK diagnostic labels. The TK copy adds an atomic
raw keyboard report for its matrix mapper, uses `tk-joystick` NVS instead of
MSX's namespace, routes audio failures to the TK platform, and filters OTA
filenames for `ESP32_TK95`. The unchanged `MsxSoftUsb` library keeps its
upstream-facing filenames and transport API names; these are dependency
references, not TK application modules. Original attribution and
non-commercial notices continue to apply. No fMSX CPU or machine code is used.

Board regression tests and their stubs were also copied locally; the
firmware-update filename tests were adapted to the TK product.

The reference is [EremusOne/ESPectrum](https://github.com/EremusOne/ESPectrum),
revision `2a2c3350eb2b26dd871fc1f98cbf4f6c93c41948`, specifically its documented
TK features, frame/raster timing constants, floating-bus coordinates and ROM
language input. ESPectrum is GPL-3.0; **its source and embedded ROM arrays
are not included**. It is a behavioral reference, not a linked dependency.

The chips Z80 is unmodified. Its complete copyright and zlib permission
notice is embedded in its header and its upstream [license](chips/LICENSE)
is retained. Third-party licenses are not replaced by the repository's
license. User ROM/media files are not redistributed.
