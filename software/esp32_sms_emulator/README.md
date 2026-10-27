# ESP32 Sega Master System

Sega Master System firmware for the Retro Hacker Clone Series ESP32-S3 N16R8
board, with VGA, PWM audio, a native USB keyboard, two low-speed USB joystick
ports, SD cartridge loading and the shared F12 configuration/update menu.

[hi631/esp32-smsemu](https://github.com/hi631/esp32-smsemu) was the architectural
reference. With the project owner's approval, this implementation uses the
MIT-licensed **TotalSMS** core instead of that reference's GPL SMSPlus and
separately restricted Z80 code. Its SPI LCD, PSX controller and flash-ROM
partition are replaced with this board's existing peripherals. See
[dependency provenance and licensing](lib/README.md).

## Hardware

| Function | GPIO |
| --- | --- |
| VGA red low / high | 5 / 4 |
| VGA green low / high | 7 / 6 |
| VGA blue low / high | 9 / 8 |
| VGA HSYNC / VSYNC | 2 / 1 |
| USB keyboard D- / D+ | 19 / 20 |
| USB joystick 1 D- / D+ | 15 / 16 |
| USB joystick 2 D- / D+ | 17 / 18 |
| SD/MMC CMD / CLK / D0 | 38 / 39 / 40 |
| Mono PWM audio | 47 |
| Onboard RGB LED (disabled) | 48 |

Use the UART connector for upload/diagnostics. Native USB is reserved for a
directly connected HID boot keyboard. The board must supply USB VBUS.
Joysticks use the same direct **low-speed USB HID** transport as the other
firmwares; full-speed-only gamepads, hubs, Bluetooth and XInput are unsupported.

## Build and test

This entire folder is standalone: the board definition, VGA/GFX/BusIO, USB
transport and emulator sources are local copies, not sibling-project symlinks.
Install PlatformIO and its pinned Espressif platform/toolchain, then run:

```powershell
pio run
make firmware
make test
```

`make firmware` builds, packages and verifies
`dist\ESP32_SMS-1.02.FLH`. `make upload` flashes through UART.
Host regressions require PowerShell plus native GCC and G++ on PATH
(for example, MSYS2 UCRT64). The tests generate their own small Z80 cartridges;
no commercial game or BIOS files are needed.

## Prepare the SD card

Use FAT32. Copy the supplied [sdcard layout](sdcard/README.md) to the card and
place your legally obtained, uncompressed `.sms` or `.rom` cartridges in
`sms\roms`. No BIOS is required, and no game ROM bytes are distributed.

- Supported payload sizes are 8 KiB or multiples of 16 KiB, up to **1 MiB**.
- An optional 512-byte copier header is removed automatically.
- File extensions are case-insensitive. ZIP archives are not supported.
- ROMs with Game Gear headers, invalid sizes, read errors or allocation
  failures are rejected without replacing the current game.
- Headerless SMS homebrew is accepted. Auto mapping uses TotalSMS's embedded
  CRC database, falling back to Sega mapping for unknown SMS cartridges.
  Choose Sega, Codemasters or Korean explicitly if a homebrew/hack needs it.
- Paths must be absolute on SD, shorter than 240 bytes, without `.` / `..`,
  empty components, control characters, colons or backslashes.

The ROM browser starts in `/sms/roms` if present, supports nested directories
and parent navigation, and pages through ten entries at a time.

## F12 menu

F12 pauses the emulated CPU and audio. Use Up/Down, Enter and Left/Right
where indicated. Esc returns one level; F12 resumes directly from submenus
and browsers when a cartridge is running. Before a cartridge is loaded,
the menu remains open.

| Option | Behavior |
| --- | --- |
| Resume emulation | Continues the current game. |
| Load ROM | Browses `.sms` / `.rom` files and asks before replacing a running game. Successful loads start immediately. |
| Configuration | NTSC/PAL timing, Export/Japan console, mapper, sound, volume, auto boot, frame skipping, save configuration and reboot. |
| USB joysticks | Enables/disables each physical port, shows connection/calibration status and VID/PID, calibrates or clears a saved mapping. |
| Console Pause | Sends the SMS console Pause/NMI, **not** the F12 host pause. |
| Reset running cartridge | Resets the current cartridge after confirmation; game progress, including session SRAM, may be lost. |
| Save boot configuration | Saves the selected ROM path, region, mapper, sound, render settings and enabled joystick ports in SMS-specific NVS. |
| Retry microSD | Retries a failed initial SD mount. Restart the board after replacing a mounted card. |
| Firmware update | Installs a compatible `.FLH` package after confirmation. |
| Hardware / help | Shows pin assignments, keyboard controls and limitations. |

Sound, volume, frame skipping and joystick enable switches apply on resume.
Timing, console region and mapper changes are **pending until Configuration >
Reboot selected cartridge** (or a new ROM load). Reset uses the running
configuration, not pending selections. Loading another ROM resets the mapper
choice to Auto. Save configuration explicitly to retain changes across power
cycles; loading a game does not silently overwrite saved boot defaults.

At startup there is a 2.5-second F12 window before the saved cartridge loads.
Missing SD, missing/invalid ROMs or invalid saved settings leave the menu
available. Saved joystick calibrations use a separate SMS NVS namespace and
do not overwrite another emulator's settings.

### Connect and calibrate joysticks

1. Plug a supported low-speed USB gamepad into port 1 or 2.
2. Open **F12 > USB joysticks**, enable its port and select **Calibrate**.
3. Release the controls, then capture Up, Right, Down, Left, button 1 and
   button 2 with Enter when prompted. Keep each pose held until the next prompt.
4. The inferred axes/hat/buttons and controller identity are saved automatically.
   Save boot configuration separately if you changed port enable settings.

Port 1 controls SMS player 1; port 2 controls player 2. Unknown or uncalibrated
devices never inject guessed input. Disconnects release controls; reconnecting
the same VID/PID uses its saved calibration. Calibration rejects identity
changes and missing fresh reports. Some unusual multi-report devices are not
supported by the transport's eight-byte, one-endpoint-per-pad limit.

### Keyboard controls

| Key | SMS function |
| --- | --- |
| Arrows | Player 1 directions |
| Z or Space / X | Player 1 button 1 / button 2 |
| W / S / A / D | Player 2 directions |
| G / H | Player 2 button 1 / button 2 |
| Enter or Pause | Console Pause |
| R | Console Reset line |
| F12 | Host menu (never forwarded to the console) |

Keyboard and enabled gamepads are combined. Console Pause has its original
game-dependent behavior; it is not a guaranteed pause screen. Enter used in
the host menu must be released before it can trigger console Pause.

## Firmware switching

This firmware uses the same dual 4 MiB OTA slots as the current CP400, MSX,
TK95, Apple II, MSX TurboR and Macintosh Plus builds. Copy trusted `.FLH`
packages to the SD root, use **F12 > Firmware update**, confirm, and keep power
connected until restart. Package filenames do not need renaming.

The updater validates the package checksum, ESP32-S3 image, OTA layout and size,
then verifies the second pass and inactive-slot image before selecting it.
The original file remains on SD. A checksum detects corruption, **not**
authenticity. FLH does not replace the bootloader or partition table; older
layouts require a one-time UART upload.

The repository's `software\Test-FirmwareCompatibility.ps1` includes SMS in
its layout and mock-OTA cross-emulator checks.

## Scope and validation limits

- Z80, SMS VDP modes, Sega/Codemasters/Korean mapping, two digital controllers,
  NTSC/PAL and SN76489 PSG audio are implemented through TotalSMS.
- SMS 256x192, 256x224 and 256x240 output fits the 320x240 framebuffer.
  Horizontal nearest-neighbor expansion fills 320 pixels; shorter modes are
  vertically centered. VGA remains 60 Hz even for PAL emulation.
- **Cartridge SRAM is session-only**: it is not loaded from or saved to SD.
  There are no save states, FM/YM2413, Game Gear/SG-1000 modes, light guns,
  paddle, 3-D glasses or BIOS menus.
- Unknown cartridge compatibility follows the selected mapper/core. No claim
  is made that every game works or that emulation is cycle-perfect.
- Host tests cover synthetic cartridge execution, video colors/scaling,
  NTSC/PAL PCM counts, mapper banking and SRAM, ROM validation/replacement,
  two-player input, actual F12 menu/browser navigation, NVS error handling,
  calibration, joystick reconnect/error behavior, PWM mute and OTA checks.
- ESP32-S3 builds and host tests do not establish physical-board performance.
  VGA/audio quality, real game compatibility, USB hardware and live firmware
  switching still need validation on the board. UART reports measured speed;
  use frame skipping if necessary.

See the [firmware change log](docs/log.md).
