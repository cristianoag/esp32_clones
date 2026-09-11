# ESP32 MSX / fMSX

This is the MSX firmware for the Retro Hacker Clone Series board. It uses the original fMSX core and targets the shared ESP32-S3 N16R8 hardware used by the rest of the project.

## Hardware

The hardware layout matches the shared board used by the CP400 project and is documented here for reference.

| Function | GPIO |
| --- | --- |
| VGA red, low/high bits | 5 / 4 |
| VGA green, low/high bits | 7 / 6 |
| VGA blue, low/high bits | 9 / 8 |
| VGA HSYNC / VSYNC | 2 / 1 |
| USB keyboard D- / D+ | 19 / 20 |
| USB joystick 1 D- / D+ | 15 / 16 |
| USB joystick 2 D- / D+ | 17 / 18 |
| SD/MMC CMD / CLK / D0 | 38 / 39 / 40 |
| Mono PWM audio | 47 |
| Onboard RGB LED | 48 |

SD uses a one-bit SD/MMC interface rather than SPI. Connect a USB HID keyboard directly to the keyboard port, and use the board's UART port for upload and diagnostics. The native USB controller is reserved for the keyboard.

The firmware reuses the existing board definition, VGA, Adafruit GFX, and BusIO libraries from the CP400 project. Keep both firmware folders in the repository when building.

## Prepare the SD card

Use a FAT32 card. BIOS files are supplied by the user and loaded from SD rather than being built into the firmware. From this firmware directory:

```powershell
.\tools\Import-Roms.ps1 -Destination .\sdcard `
    -OmegaRom 'C:\path\to\omega_msx2+_all_ntsc.bin' `
    -ExpertRom 'C:\path\to\expert_1.1_basic-bios1.rom' `
    -HotbitRom 'C:\path\to\hotbit_1.2_basic-bios1.rom'
```

Copy the generated `sdcard\msx` folder to the card root. Alternatively, use the SD drive root as the destination.

The initial profiles are:

| Profile directory | Machine | Default RAM | BIOS files |
| --- | --- | --- | --- |
| `msx\bios\omega` | MSX2+ | 512 KiB | `OMEGA.ROM` |
| `msx\bios\expert` | Gradiente Expert 1.1 / MSX1 | 64 KiB | `MSX.ROM` |
| `msx\bios\hotbit` | Sharp Hotbit 1.2 / MSX1 | 64 KiB | `MSX.ROM` |
| `msx\bios\fs-a1wsx` | Panasonic FS-A1WSX / MSX2+ | 64 KiB | `PANASONIC.ROM` |
| `msx\bios\fs-a1f` | Panasonic FS-A1F / MSX2 | 64 KiB | `PANASONIC.ROM` |
| `msx\bios\fs-a1fx` | Panasonic FS-A1FX / MSX2+ | 64 KiB | `PANASONIC.ROM` |

## Notes

Once VGA is ready, the startup page shows a progress gauge for menu buffers,
keyboard, audio, microSD, BIOS scanning, saved settings and joystick startup.
The percentage counts completed initialization stages, not elapsed time or
ROM-loading bytes. Warnings remain visible, and the F12 startup selection
window is unchanged.

Rendering adapts to keep emulation near the normal 50/60 Hz rate without
changing the Z80 clock. Sustained overload is checked every five PAL or six
NTSC frames, so a heavy cartridge should no longer spend several seconds
gradually reducing rendering load. F12 resume keeps the learned rendering
level. This can trade display smoothness for speed; it cannot guarantee full
speed for every ROM. The UART `MSX speed:` lines show the measured emulated
and presented frame rates.

This project keeps the original hardware and software assumptions in mind, but it is still an emulator build on modern hardware. That means the goal is a working, usable system rather than a perfect cycle-accurate reproduction of every board detail.

For setup, build, and firmware update details, read the rest of this folder's documentation and use the project notes in the repository as the main reference.
