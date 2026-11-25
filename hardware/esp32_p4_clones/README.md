# ESP32-P4 Clone Board

KiCad 10 project for the ESP32-P4 version of the shared clone board. It reuses the
outline, mounting holes and connector positions of
[`../esp32_s3_clones`](../esp32_s3_clones) (110 x 60 mm), but carries a Waveshare
[ESP32-P4-Core-DEV-KIT](https://docs.waveshare.com/ESP32-P4-Core-DEV-KIT) instead of the
ESP32-S3 dev board, and has **stereo** audio.

Open `esp32_p4_clones.kicad_pro`. The libraries are local to the project
(`libraries/`), so no global library setup is needed.

## What changed from the S3 board

| Area | S3 board | P4 board |
| --- | --- | --- |
| Module | ESP32-S3 dev board, 2 x 22 pins | ESP32-P4-Core-DEV-KIT, 2 x 20 pins (28.62 x 58 mm), USB-C end towards the board edge |
| Audio | Mono, one filtered output feeding tip and ring | Stereo: two identical RC chains (330 Ω / 47 nF, 330 Ω / 100 nF, 10 µF coupling) feeding tip (left) and ring (right) |
| USB-A J7 | GPIO software host | Native USB 2.0 high-speed host (`DM`/`DP`) |
| Power into the module | `5V` pin | `VSYS` pin (the dev kit's own USB-C ports can also power the board) |
| Reset button J5 | `RST` | `RUN` (chip enable) |
| SD card | `DAT3` unconnected | `DAT3` pulled up with R4 (CLK has no pull-up) |

New or renumbered parts: C3 (10 µF), C5 (100 nF), C7 (47 nF), R3 and R6 (330 Ω) for the
right audio channel. Everything else keeps its S3 reference designator and footprint.

## GPIO map

| Function | Dev kit pin | Net |
| --- | --- | --- |
| VGA red (1.2 kΩ / 560 Ω) | GPIO4 / GPIO5 | `VGA_R0` / `VGA_R1` |
| VGA green (1.2 kΩ / 560 Ω) | GPIO9 / GPIO10 | `VGA_G0` / `VGA_G1` |
| VGA blue (1.2 kΩ / 560 Ω) | GPIO11 / GPIO13 | `VGA_B0` / `VGA_B1` |
| VGA HSYNC / VSYNC | GPIO3 / GPIO2 | `VGA_HS` / `VGA_VS` |
| Audio left / right | GPIO48 / GPIO47 | `AUDIO_L` / `AUDIO_R` |
| SD card CLK / CMD / DAT0 | GPIO46 / GPIO33 / GPIO32 | `SD_CLK` / `SD_CMD` / `SD_D0` |
| USB-A J7 (native) D- / D+ | DM / DP | `USB_DM` / `USB_DP` |
| USB-A J6 D- / D+ | GPIO26 / GPIO27 | `J6_DM` / `J6_DP` |
| USB-A J8 D- / D+ | GPIO20 / GPIO21 | `J8_DM` / `J8_DP` |
| Reset button | RUN | `RESET` |

GPIO2 to GPIO13 and GPIO20 to GPIO48 used above are all on the dev kit's 2.54 mm headers.
GPIO6, GPIO12, GPIO22, GPIO23, GPIO53 and GPIO54 are left unconnected. GPIO7 and GPIO8 are
the dev kit's I2C pins with 2.2 kΩ pull-ups, so they are also left free. `VBUS` and `EN`
are not connected.

## Check before ordering boards

- **Header row spacing is assumed to be 25.4 mm.** Waveshare publishes the board size
  (28.62 x 58 mm), the 2.54 mm pitch and the first pin 4.87 mm from each end, but not the
  distance between the two rows. Measure your dev kit and adjust the footprint
  (`libraries/footprints/ESP32.pretty/ESP32-P4-Core-DEV-KIT.kicad_mod`) if it differs.
- The dev kit's USB-C overhang and the 3D model (`libraries/ESP32-P4-Core-DEV-KIT.py`
  generates the STEP) are approximations.
- The PJ-307 jack pads are named `G`, `S` and `T`. They are wired as sleeve = `G`,
  tip (left) = `T`, ring (right) = `S`. If the channels come out swapped on your jack, swap
  `JACK_L` and `JACK_R`.
- The board was auto-routed with Freerouting and passes ERC and DRC. Review the routing,
  especially the J7 USB D+/D- pair, which is not length matched or impedance controlled.
- Do not feed 5 V into both the barrel jack and the dev kit's USB-C ports at the same time
  unless you are sure the supplies are compatible.

The remaining DRC warnings are silkscreen clearances on the connector footprints inherited
from the S3 board.
