# ESP32 Clone Series - Quick Start

Welcome! Your ESP32 Clone Series computer can recreate several classic
computers and a games console on the same small board. Connect it to a VGA
monitor, add a keyboard, and enjoy some retro computing.

For SD-card preparation and game-loading instructions for every firmware,
see the [User Manual](user-manual.md).

## 1. Connect the computer

Make the connections with the computer switched off.

1. **Monitor:** connect a VGA cable between the computer's 15-pin VGA
   connector and a VGA monitor. Turn on the monitor and select its VGA/PC
   input. On a widescreen monitor, choose **4:3** or **Aspect** rather than
   stretching the picture.
2. **Keyboard:** plug a wired USB keyboard directly into the **rightmost
   USB-A connector**, looking at the computer from the side with the three
   USB sockets.
3. **Joysticks, if wanted:** the **other two USB-A connectors** are joystick
   ports, not extra keyboard ports.
4. **microSD:** insert the prepared FAT32 card containing the current
   firmware's ROMs and your software. Leave it inserted while using the
   computer.
5. **Power:** connect the supplied **5 V power lead** to the **2.1 mm,
   center-positive** power connector. Use the supplied power equipment;
   do not substitute a 9 V or 12 V adapter.

Looking at the side with the buttons and three USB-A sockets:

```text
[Power on/off] [Reset]     [Joystick 1] [Joystick 2*] [KEYBOARD]
 left button   right                                  always
               button                               far right
```

*Macintosh Plus uses joystick port 2, the middle USB-A socket, for a
compatible mouse. Joystick port 1 is disabled in that firmware. The
experimental MSX TurboR firmware currently uses keyboard controls only.*

VGA carries **video only**. For sound, connect the computer's 3.5 mm audio
output to powered speakers or a suitable audio input.

## 2. Turn it on

Operate the **left button** to turn the computer on. It starts the firmware
that was last installed, not a list of all the available systems.

Depending on the firmware and saved settings, you will see a BASIC prompt,
a game, a Macintosh startup screen, or the configuration menu. Press
**F12** to open the menu. On a keyboard with a shared function-key row, you
may need **Fn + F12**.

## 3. F12 is your control center

**Every firmware has an F12 menu.** Its exact options differ by system,
but this is where you manage media, settings and firmware switching.
Opening it pauses the emulated machine; it does not reset your game.

| Control | What it does |
| --- | --- |
| Up / Down | Move through the menu. |
| Enter | Select an option or open a file browser. |
| Left / Right | Change settings where indicated. |
| Esc | Go back; at the main menu, return to the running machine. |
| F12 | Open the menu; most firmwares also use it to resume from submenus. In CP400, use Esc or **Exit menu** to return. |

Use **Media**, **Disk drives**, or **Load ROM**, depending on the firmware,
to choose software. Some systems need a BASIC loading command or a cold
boot after selecting a file: the [User Manual](user-manual.md#loading-games-by-firmware)
explains each one.

## 4. Using joysticks

Use directly connected **low-speed USB HID** joysticks/gamepads.
**Low-speed** describes the USB connection standard, not how fast your
game runs. A USB plug alone does not guarantee compatibility.

- Full-speed-only pads, USB hubs, Bluetooth receivers and XInput-only
  controllers are not supported by the joystick ports.
- When available, open **F12 > USB joysticks**; CP400 calls this
  **Joystick setup**. Calibrate the controller by following the on-screen
  prompts, including the centered position, directions and fire buttons.
- Calibration and controller settings can differ between firmwares.
  Recheck them after switching systems.
- Games may require a particular joystick port or interface. Many C64
  games use **port 2**; TK games may ask for **Kempston** or **Sinclair**.

Use a game's keyboard controls if you do not have a compatible joystick.
The [manual's controller section](user-manual.md#joysticks-and-mouse)
explains the Macintosh mouse and TurboR exceptions.

## 5. Reset or change systems

### Start over

Press the **right button** once to reset the computer. This restarts the
currently installed firmware and loses the running game's unsaved state.
Never reset during a firmware update or disk write.

For a software reset, open F12 and use the firmware's reset/reboot option.
Where offered, **Warm reset** keeps RAM; **Cold boot** starts the emulated
machine afresh and applies pending machine/ROM changes.

### Switch to another firmware

1. With power off, prepare the destination system's required ROMs/media on
   the card and copy its trusted **`.FLH` firmware package** to the card root.
2. Start the computer and open **F12 > Firmware update**. TurboR labels
   this **Firmware update / switch emulator**.
3. Select the destination package and confirm the installation.
4. **Keep power connected and do not reset or remove the card.** Wait for
   installation to finish and the computer to restart into the new system.

This installs another firmware; it is not an instant switch between
already running cores. To switch back, install the previous system's FLH
through the same F12 menu. Its package stays on the card.

These steps require current firmware and the shared partition layout.
For older installations, see
[Firmware switching and updates](user-manual.md#firmware-switching-and-updates).

## 6. Finish safely

Finish saving inside the game or operating system and wait for disk
activity to stop. In TurboR, also open F12 to flush its persistent SRAM/CMOS
files. Then use the **left button** to turn the computer off. Remove the
microSD card only after power is off.

**Ready to play?** Choose your system in the
[game-loading guide](user-manual.md#loading-games-by-firmware).
