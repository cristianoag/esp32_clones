# Macintosh Plus on the ESP32 Clone Series board

Standalone PlatformIO firmware integrating the Musashi 68000 and Macintosh
peripheral core from [epatel/esp32_mac](https://github.com/epatel/esp32_mac)
with this repository's shared board, F12 menu and firmware-update conventions.

## Hardware

| Function | GPIO / behavior |
| --- | --- |
| VGA | HSYNC 2, VSYNC 1; red 5/4, green 7/6, blue 9/8 |
| Display | 640x480 RGB222; centered native 512x342 monochrome Mac image |
| Keyboard | Native USB host: D-19 / D+20 |
| Mouse | **Joystick port 2**: D-17 / D+18 |
| Joystick port 1 | Disabled in this firmware |
| SD | CMD 38, CLK 39, D0 40; native SDMMC 1-bit |
| Mono audio | GPIO47 PWM; GPIO48 RGB LED switched off |
| Memory | 16 MiB flash, 8 MiB OPI PSRAM; 4 MiB emulated Mac RAM |

The board supplies USB VBUS. Use the UART connector for uploads and the
115200-baud console, not the native USB keyboard connector.

### Mouse compatibility

Use a **direct low-speed HID boot mouse** in the second joystick connector.
This is the agreed existing-board implementation: the software USB host
cannot operate full-speed-only mice, hubs, Bluetooth dongles or arbitrary
vendor-specific HID devices. Full-speed mice require different host hardware
or wiring. The native USB controller remains dedicated to the keyboard.

The mouse interface is identified from its configuration and explicitly put
into boot protocol. Signed relative X/Y and the left button are translated
to Mac quadrature signals and the VIA button input; other buttons and wheel
are ignored. Disconnect releases the button and clears motion. F12 discards
menu-time movement. Flash writes pause the software USB bus and force
re-enumeration. The help screen shows connection status, and UART reports
unsupported devices. Configuration descriptors must fit 255 bytes.

### Keyboard

Direct HID boot keyboards use the same native host transport as the other
firmwares. GUI/Windows keys map to Command, Alt to Option, either Shift to
Shift and either Ctrl to its legacy scan code. Caps Lock is latched.
Letters, digits, punctuation, editing keys, arrows and keypad
digits/Enter/decimal are translated to M0110A events, including extended
arrow/keypad prefixes. The Mac OS supplies repeat. F12 is exclusively a host
menu key. Keypad arithmetic and F1-F11 have no mapping in this release.

## SD card and ROM

Format the card FAT32 and supply:

```text
macplus/
  bios/
    macplus.rom
  disks/
    hd.img
ESP32_MACPLUS-1.00.FLH
```

Use your own **unmodified, raw 128 KiB Macintosh Plus ROM**. Its word-sum
checksum and reset-vector prefix are checked before execution. Other Mac
models, byte-swapped dumps and patched large-screen ROMs are unsupported.
No Apple ROM, operating system, application or disk image is bundled or
downloaded by the build. The native display does not require ROM patches.

`hd.img` must be a **whole SCSI disk** with its partition map, compatible
Macintosh SCSI driver and installed bootable system. It appears as SCSI
ID 6. A floppy image or bare HFS volume suitable for Mini vMac is not a
whole SCSI disk and will not boot through this controller. Prepare a whole
disk in a compatible emulator such as MAME, install the system and disk
driver there, then copy the resulting raw image to the card.

Images must be sector-aligned (512 bytes) and between 512 bytes and 1 GiB.
Transfers are bounded by the controller's 32 KiB buffer (64 sectors per
command); larger requests fail explicitly. Reads, writes, capacity, inquiry,
request-sense and basic mode-sense are implemented. **Read/write is the
default**, and writes are flushed to SD. Keep a backup. F12 can select
read-only access for the next cold boot. Runtime I/O faults are reported and
open the F12 menu.

Clearing the disk assignment boots with no SCSI disk, leaving the Mac's
missing-boot-media screen. IWM floppy insertion is not implemented.
The `macplus` folder can coexist with other emulators' SD folders.

Create the local SD layout and a blank 100 MiB image, or import your own
authorized ROM and already prepared disk:

```powershell
.\tools\Import-SdCard.ps1 -BlankDiskMiB 100
.\tools\Import-SdCard.ps1 -Rom 'C:\MyMac\Mac-Plus.ROM' -Disk 'C:\MyMac\bootable.img'
```

The importer checks ROM size/checksum and verifies copied files with SHA-256.
It refuses to replace differing files unless `-Force` is explicitly supplied;
an existing disk is preserved when no replacement is provided. A newly
created blank image has **no partition map, driver or operating system** and
is not bootable. The entire local `sdcard` folder and ROM/IMG extensions are
excluded from Git. Never force-add these private assets.

## F12 menu

F12 pauses CPU, disk access and sound. Arrows select, Enter applies and
Esc/F12 resumes an already running machine.

- Resume or boot the Macintosh.
- Browse SD directories for a `.rom` or whole-disk `.img`.
- Delete on the SCSI row clears the next-boot disk assignment.
- Set disk read-only/read-write access for the next cold boot.
- Toggle sound and adjust volume with left/right.
- Toggle automatic boot with Enter; left/right on that row selects
  rendering every 1-4 frames, without skipping emulated CPU time.
- Save the complete boot configuration explicitly.
- Cold boot the selected configuration, with confirmation.
- Install any compatible emulator's `.FLH` package.
- View hardware, keyboard and mouse help.

ROM, disk and protection selections are pending until cold boot; no mounted
system disk is hot-swapped. Saving settings does not reboot. PRAM is stored
separately under the `macplus` NVS namespace, periodically saved when changed
and flushed before intentional cold boot or update. Other emulators'
settings are preserved. F12 during the 2.5-second startup delay bypasses
automatic boot. Missing media or invalid settings leave the menu available.

## Build and firmware switching

Install PlatformIO, GNU make and PowerShell, then from this directory:

```powershell
make firmware
make test
make upload
```

`make firmware` builds and verifies `dist\ESP32_MACPLUS-1.00.FLH`.
The platform is pinned to `espressif32@6.11.0`. Board definitions and libraries
are local, so the directory can be compiled independently.
`make test` also requires native `gcc` and `g++` on PATH.

The partition table is the shared dual 4 MiB OTA layout. FLH framing and the
updater match CP400, MSX, TK95 and Apple II: any compatible `.FLH` filename is
accepted. Packages are fully validated before erasing the inactive slot,
checked again during writing, verified as an ESP32-S3 application and only
then selected for boot. Failed installations do not select partial images.
Source FLH files are preserved.

Copy packages to the SD root (required by CP400's browser), then use
**F12 > Firmware update** to switch in either direction without renaming.
Older updaters may need an update to the current version of their own
firmware first. The old CP400 partition layout requires a one-time UART
upload of the shared layout. FLH does not replace bootloaders or partition
tables. Checksums are not signatures: install only trusted packages built
for this board and keep power connected during updates.

From the repository root, after building the firmwares registered by the
compatibility script, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\software\Test-FirmwareCompatibility.ps1
```

This checks partition layouts and all registered source/destination
combinations against mock OTA storage.

## Scope and validation

This is an initial integration, not a claim of complete cycle accuracy or
full-speed emulation. CPU budgets target 7.8336 MHz and 60.15 Hz; actual speed
depends on PSRAM, VGA and SD performance and is reported on UART. Audio is
sampled from the Macintosh sound buffer through the shared mono PWM driver.
There is no floppy-media support, LocalTalk, serial networking, sleep,
save-state support or ROM-screen patching.

Native tests use generated 68000 instructions, not Apple ROM bytes, and cover
the bus, ROM protection, buffer selection, pixel polarity, audio, keyboard,
mouse, settings, SCSI and updater failures. Build and host-test success does
**not** establish that a real Mac ROM boots an installed system, that a
particular mouse enumerates, or that VGA, audio and firmware switching work
on the physical board. Those hardware and user-ROM/disk checks remain
necessary before this can be treated as a verified hardware release.

## References and provenance

- [epatel/esp32_mac](https://github.com/epatel/esp32_mac), revision
  `eaffb010eee2267672f1f766fcc4c5eb80cb5baa`: imported peripherals and bundled
  Musashi, adapted bus, stepping, resets, interrupts, keyboard, bounds and RTC.
  Its Waveshare LCD, BLE input, flash disk and screen patches are not used.
- [Spritetm/minimacplus](https://github.com/Spritetm/minimacplus): original
  Macintosh peripheral core underlying that port.
- [kstenerud/Musashi](https://github.com/kstenerud/Musashi): 68000 CPU core;
  this project uses the version bundled by epatel.
- [TMK M0110 protocol](https://github.com/tmk/tmk_keyboard/wiki/Apple-M0110-Keyboard-Protocol):
  model/test commands, raw scan codes and arrow/keypad prefixes.
- [Mac Plus hardware notes](http://www.mac.linux-m68k.org/devel/plushw.php.html):
  VIA screen and sound buffer selection.
- [Existing Apple II firmware](../esp32_apple2_emulator/README.md): board
  drivers, native keyboard transport, software USB scheduling/flash pause,
  mono audio, menu conventions, FLH updater and tests. The new local software
  USB copy selects a boot mouse on port 2 rather than a gamepad.
- [bitluni/ESP32-S3-VGA](https://github.com/bitluni/ESP32-S3-VGA),
  [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) and
  [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO): library copies
  reused from this repository.

Original notices are retained in vendored sources. This list is provenance,
not license clearance; review upstream and repository-specific terms before
distribution. See the [firmware change log](docs/log.md).
