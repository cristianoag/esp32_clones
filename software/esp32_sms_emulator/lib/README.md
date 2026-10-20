# Local dependencies and provenance

All build inputs are local to this firmware folder. Dependency licenses remain
in their original files and are not replaced by the repository's root license.
No commercial ROMs or BIOS images are included.

| Dependency | Origin / revision | Terms |
| --- | --- | --- |
| TotalSMS core | [ITotalJustice/TotalSMS](https://github.com/ITotalJustice/TotalSMS), commit `05bbb8bbd931d21653cbb6dd7a75a5b198c86c29` | [MIT](TotalSMS/LICENSE) |
| SN76489 PSG | [ITotalJustice/sn76489](https://github.com/ITotalJustice/sn76489), `v1.2.1`, commit `7da8d06e315878b56a7fe9fc641b8c499cc5336e` | [MIT](TotalSMS/LICENSE-sn76489.txt) |
| Scheduler | [ITotalJustice/scheduler](https://github.com/ITotalJustice/scheduler), `v1.1.0`, commit `ef78903bb91b681cd0febafb632c3f289a489ede` | [MIT](TotalSMS/LICENSE-scheduler.txt) |
| blip_buf C resampler | Shay Green, as shipped with that PSG revision | MIT notice in [blip_buf.c](TotalSMS/src/blargg/blip_buf.c) |
| Board definition | Existing TK95 N16R8 definition | Existing repository terms |
| VGA | Existing TK95 copy of bitluni ESP32 S3 VGA 0.1.0, including board adaptations | Original notices retained |
| GFX / BusIO | Existing TK95 copies, Adafruit GFX 1.12.1 / BusIO 1.17.1 | Original licenses retained in their directories |
| Low-speed USB | Existing TK95 copy of `MsxSoftUsb` and its Dmitry Samsonov/tobozo transport | Original notices retained |
| Native keyboard / joystick / PWM / updater integration | Adapted from this repository's TK95 firmware; runtime joystick tests from MSX | Existing source notices and non-commercial terms retained |

The TotalSMS copy contains only the core, its required C PSG/scheduler and
the MIT C resampler. It excludes upstream frontends, ROM assets and the
alternative C++ Blip_Buffer implementation.

Local TotalSMS changes:

- Bounded ROM-header probes by the supplied file length.
- Accepted 8/16/48 KiB and 1 MiB header size codes and explicit SMS headerless homebrew.
- Prevented a zero bank divisor for 8 KiB cartridges.
- Mirrored small cartridge mappings and bounded bank addresses by ROM size.
- Corrected the exact-timing setter so disabling exact timing works.
- Enabled the existing 224/240-line rendering paths with a 240-row frontend
  framebuffer, removing upstream placeholder assertions and testing row bounds.

The wrapper caps SMS cartridges at 1 MiB, strips optional copier headers,
rejects Game Gear headers, stages replacement cores before committing and
adapts pixels, PCM and input to the board. Its own platform-specific source
lives in `../src`, not inside the upstream core.

Architectural reference:
[hi631/esp32-smsemu](https://github.com/hi631/esp32-smsemu), commit
`0d883ba029c479d057a8c35ea88b0f08c4bf36f8`. Its README describes SMSPlus
as GPL2; core files retain GPL2-or-later notices and its Z80 source has separate
non-commercial terms. The repository-level Apache license does not erase
those per-file terms. **No code from that reference is compiled or vendored
here.** TotalSMS was selected with the project owner's approval to avoid
combining that GPL core with this board support's non-commercial restrictions.

Copies are independent, not symlinks. Fixes in a sibling firmware must be
reviewed and ported explicitly.
