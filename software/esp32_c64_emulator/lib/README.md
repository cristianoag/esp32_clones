# Dependencies

All firmware dependencies are local. See
[licenses, pinned upstream revision and authorization](../docs/licenses.md).

`retroelec` contains only the CPU6502, CIA, VIC and SID modules from the
upstream C64 project, with original notices. `Config.h` and
`platform/PlatformManager.h` are minimal local adapters. No upstream
ROMs or game data are included. Hardware libraries and board adapters
follow the existing Clone Series layout.
