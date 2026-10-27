# ESP32 Clone Series - User Manual

This manual covers everyday use of the shared ESP32-S3 Clone Series
computer, preparing a microSD card, and loading software in all eight
current firmwares. For the shortest connection checklist, use the
[Quick Start](quick-start.md).

The computer runs **one installed firmware at a time**. In this guide,
*core* means the emulated system inside that firmware. Changing a machine
profile within one firmware is different from installing another firmware.

All firmwares are under active development. CP400 and ordinary MSX are the
most mature; other cores have narrower compatibility, and MSX TurboR is
experimental. Supported file formats do not guarantee that every game
works or runs at full speed.

## Contents

- [Connections and power](#connections-and-power)
- [The common F12 menu](#the-common-f12-menu)
- [Resetting, saving and shutting down](#resetting-saving-and-shutting-down)
- [Joysticks and mouse](#joysticks-and-mouse)
- [Preparing the microSD card](#preparing-the-microsd-card)
- [Loading games by firmware](#loading-games-by-firmware)
  - [CP400 / CoCo 2](#cp400--coco-2)
  - [MSX / MSX2 / MSX2+](#msx--msx2--msx2)
  - [MSX TurboR / FS-A1ST](#msx-turbor--fs-a1st-experimental)
  - [TK90X / TK95](#tk90x--tk95)
  - [Apple II+ / original Apple IIe](#apple-ii--original-apple-iie)
  - [Macintosh Plus](#macintosh-plus)
  - [Sega Master System](#sega-master-system)
  - [Commodore 64](#commodore-64)
- [Firmware switching and updates](#firmware-switching-and-updates)
- [Troubleshooting](#troubleshooting)
- [Firmware-specific references](#firmware-specific-references)

## Connections and power

### Find the connectors

Directions below assume you are **facing the side with the three USB-A
sockets**, with the computer upright.

```text
[Power on/off] [Reset]     [Joystick 1] [Joystick 2] [Keyboard]
 left button   right        leftmost     middle      rightmost
               button         USB         USB          USB
```

The **keyboard always goes in the rightmost USB-A connector**, regardless
of the installed firmware. Connect it directly, not through a hub.
The two other sockets are the low-speed joystick ports; their
firmware-specific roles are described under [Joysticks and mouse](#joysticks-and-mouse).

### Connect a VGA monitor

1. With the computer off, connect a VGA cable to its 15-pin VGA connector
   and to the monitor.
2. Turn the monitor on and select **VGA**, **PC**, or the equivalent input.
3. Select the monitor's **4:3** or **Aspect** mode to avoid a stretched image.
   Use its auto-adjust control if the picture is shifted.

The board outputs analog VGA, not HDMI. An HDMI-only display needs a
compatible **active VGA-input to HDMI-output converter**; a passive cable
or an HDMI-to-VGA converter in the opposite direction is not a substitute.
Direct VGA is the simplest starting point.

Image resolution varies by firmware. Selecting PAL/NTSC or 50/60 Hz
*emulation* does not necessarily change the VGA signal sent to the monitor.
TurboR additionally offers **VGA fit** in F12 if the display clips edges.

### Audio, card and power

- Connect the **3.5 mm mono audio output** to powered speakers or a suitable
  audio input. VGA does not carry sound. Use a modest speaker volume first.
- Insert the prepared **FAT32 microSD card** before starting. ROMs and
  mounted media may be read throughout use: leave the card inserted.
- Connect the **supplied 5 V power lead** to the **2.1 mm center-positive
  barrel connector**. Use the supplied power equipment, not a 9 V/12 V adapter.
- Operate the **left button** to turn the computer on or off.
- The **right button resets** the computer; it is not a firmware selector.

The ESP32 module's UART/programming connection is for maintenance and
diagnostics, not the normal keyboard connection. Routine use and firmware
switching do not require connection to a PC.

## The common F12 menu

**F12 opens the host menu in every firmware.** It pauses emulation so you
can manage the computer without sending those menu keystrokes to the game.
It is separate from a game's own menu or pause control. Some PC keyboards
require **Fn + F12**.

The interface follows common conventions, but **the menu entries are not
identical in every firmware**. Read the hints at the bottom of the screen.

| Key | Usual action |
| --- | --- |
| Up / Down | Select a row. |
| Enter | Apply/select, or open the selected file browser. |
| Left / Right | Change a setting where indicated. |
| Esc | Go back one page; from the main menu, resume a running machine. |
| F12 | Open the menu; most firmwares also resume from ordinary submenus. CP400 uses Esc / **Exit menu** to leave. |
| Page Up / Page Down | Change file-browser pages where supported. |
| Delete | Eject or clear the selected media assignment where offered. |
| Y / N | Confirm/cancel actions where prompted; Esc/F12 cancels many confirmation dialogs. |

Common functions include media selection, configuration and **Firmware
update**. Reset, audio and input controls depend on the core.
Most firmwares provide a brief F12 interception window before automatic
boot; pressing F12 during it lets you change settings before software starts.

### Selecting, applying and saving are different

- **Selecting media** chooses a cartridge, disk, tape or program. Whether
  it takes effect immediately depends on the core and media type.
- **Cold boot/reboot** applies pending ROM, cartridge or machine changes
  where required. Merely closing F12 does not apply those changes.
- **Save boot configuration** remembers selections and preferences for
  the next startup. It is **not a game save or a snapshot** of the running CPU.
- **Reboot without saving** uses the selected settings for that session.
  It does not undo writes already made to a writable disk image.

Most cores keep settings and calibration in separate internal namespaces;
CP400 keeps its configuration on SD. Do not expect one core's controller
calibration or boot preferences to configure another core.

## Resetting, saving and shutting down

| Action | Effect |
| --- | --- |
| Physical right-hand reset button | Restarts the ESP32 and currently installed firmware; unsaved runtime state is lost. |
| F12 **Warm reset**, where available | Resets the emulated machine while retaining RAM; details vary by core. Not a save feature. |
| F12 **Cold boot** / reboot | Restarts the emulated machine and applies selected boot configuration; assume current progress will be lost. |
| F12 **Firmware update** | Installs an application and restarts into that firmware. |

CP400 has **Reboot CP400**; ordinary MSX offers **Boot BIOS + slots + media
(cold reset)** and Media reboot choices; SMS has **Reset running cartridge**.
Not every core has a warm-reset option.

Before turning off:

1. Save through the game's or operating system's own save command, where
   the core and media support it. Finish any operating-system shutdown
   procedure before cutting power.
2. Wait for disk activity to finish. Do not reset, switch firmware, remove
   the card or cut power during writes.
3. In TurboR, enter F12 to flush firmware/cartridge SRAM and CMOS files.
4. Use the **left power button** to turn off. Remove the card only afterward.

Keep backups of writable images. Apple II disks are read-only, C64 D64
support is extraction-only, and SMS cartridge SRAM is session-only; those
limitations cannot be bypassed by saving the F12 boot configuration.

## Joysticks and mouse

### USB compatibility

The two joystick sockets use a software USB host restricted to **low-speed
USB HID** devices. Low-speed is the USB signaling mode, **not a setting
that makes games run slowly**.

- Use a compatible wired controller connected directly to a joystick socket.
- Full-speed-only gamepads, hubs, Bluetooth dongles, and XInput-only
  controllers are not supported by these ports.
- A controller marketed as "USB", "retro", or "PC compatible" is not
  automatically compatible. Complex HID/report formats may also fail.
- The shared gamepad transport has an eight-byte input-report limit.
  Calibration cannot make an unsupported USB device enumerate.

If a controller is not detected, try a known low-speed HID pad rather than
moving the keyboard out of its dedicated rightmost port.

### Calibrate a supported controller

1. Connect the pad to the leftmost or middle socket.
2. Open **F12 > USB joysticks** or, in CP400, **Joystick setup**.
3. Enable the port if that firmware offers an enable setting, then select
   its calibration action.
4. Follow the displayed order exactly: release/center all controls, then
   capture each requested direction and the two fire buttons with Enter.
   **CP400's direction order differs from several other cores.**
5. Hold each requested control until the next prompt. Check live input if
   the firmware provides it, then return to the game.
6. Save boot settings separately if required after changing port enable or
   interface choices. Calibration may already have saved automatically.

Repeat in another firmware as needed. Choose the game's expected port or
joystick interface, not just a physical socket that happens to be free.

| Firmware | Input notes |
| --- | --- |
| CP400 / CoCo 2 | Two calibrated USB joystick ports; follow the game's controller requirements. |
| Ordinary MSX | Two calibrated joystick ports; games commonly offer joystick or keyboard control. |
| MSX TurboR | The current experimental board integration does **not** feed USB joysticks into the core. Use keyboard-compatible software. |
| TK90X / TK95 | Choose Kempston, Sinclair 1, Sinclair 2 or Cursor to match the game. Fire A is the emulated fire button. Two pads on Kempston merge; Sinclair 1/2 can provide separate players. |
| Apple II | Port 1 maps to paddles 0/1; port 2 to paddles 2/3 with shared button behavior. Directional input gives minimum/center/maximum positions, not continuous analog control. |
| Macintosh Plus | **Middle socket / joystick port 2 is the mouse port**; joystick port 1 is disabled. |
| Sega Master System | Physical ports 1/2 control players 1/2 with two game buttons each. Keyboard controls are also available. |
| Commodore 64 | Physical ports map to C64 ports 1/2. Many games need port 2; use **Swap joystick ports** if playing from physical port 1. Only fire A has a C64 fire line. |

### Macintosh mouse

Use a directly connected **low-speed HID boot mouse** in the **middle
USB-A socket**. Full-speed-only mice, hubs, Bluetooth receivers and
vendor-specific devices are not supported. Only movement and the **left
button** are translated; the wheel and other buttons are ignored.
The keyboard still belongs in the rightmost socket.

## Preparing the microSD card

### Files and folders

Use **FAT32**. Back up a card before formatting it: formatting erases its
contents. On a PC, copy the required files onto the card and safely eject
it before inserting it into the powered-off computer.

One card can hold all cores' folders and firmware packages. The following
is an organization example, not a set of bundled games:

```text
Card root\
  ESP32_CP400-<version>.FLH
  ESP32_MSX-<version>.FLH
  ...other trusted firmware packages...
  cp400\
    bios\
    disks\
      YOURGAME.DSK
  msx\
    bios\
    audio\
    roms\
    disks\
    tapes\
  msxturbor\
    bios\fs-a1st\
    roms\
    disks\
  tk\
    bios\
    tapes\
    snapshots\
  apple2\
    bios\
    disks\
  macplus\
    bios\
    disks\
  sms\
    roms\
  c64\
    bios\
    programs\
```

Copy a firmware project's staged system folder to the **card root**,
not inside an extra `sdcard` folder. For example, the card should contain
`cp400\bios`, not `sdcard\cp400\bios`.
Supported browsers can generally traverse folders, including CP400's
disk-image picker. Keep filenames and paths short and simple.

Use legally obtained BIOS ROMs and software. The project does not supply
commercial BIOS, operating systems or games. Renaming a ZIP, HTML page,
audio recording or unsupported disk format does not convert it to a
supported image: extract archives on the PC and use the actual media file.

### Boot ROM checklist

These are the default paths; some firmwares let you select alternatives.
Only prepare the profiles you intend to use.

| Firmware | Required boot files on the card |
| --- | --- |
| CP400 | `cp400\bios\extbas11.rom` and `bas13.rom`, plus the selected `cp400dsk.rom` or `disk11.rom`; each is 8,192 bytes. |
| MSX | A supported profile under `msx\bios`: Omega uses `omega\OMEGA.ROM`; Expert/Hotbit use `expert\MSX.ROM` / `hotbit\MSX.ROM`; Panasonic profiles use their own `PANASONIC.ROM`. See the [MSX setup reference](../software/esp32_msx_emulator/README.md#prepare-the-sd-card) for imports. |
| MSX TurboR | `msxturbor\bios\fs-a1st\fs-a1st_firmware.rom` (4,194,304 bytes) and `fs-a1st_kanjifont.rom` (262,144 bytes). FS-A1GT firmware is not supported. |
| TK90X / TK95 | Selected model's `tk\bios\tk90.rom` or `tk95.rom`, exactly 16,384 bytes. Default model is TK95. |
| Apple II | Selected model's `apple2\bios\apple2plus.rom` (12,288 bytes) or `apple2e.rom` (16,384 bytes), plus `disk2.rom` (256 bytes). Use an **original**, not enhanced, IIe ROM. |
| Macintosh Plus | `macplus\bios\macplus.rom`, an unmodified raw 131,072-byte Plus ROM, and a prepared bootable whole-SCSI-disk image for normal use. |
| Sega Master System | No BIOS required. Supply a supported game cartridge file. |
| Commodore 64 | `c64\bios\basic.rom` and `kernal.rom` (8,192 bytes each), plus `chargen.rom` (4,096 bytes). |

Matching a filename and size alone does not establish that a ROM is the
correct dump. Firmware-specific import tools and validation requirements
are documented in the [references](#firmware-specific-references).

## Loading games by firmware

Menu labels below describe current builds. Replace example filenames with
the actual names on your card; commands shown are examples, not universal
launchers for every game. Press Enter after each BASIC command.

### CP400 / CoCo 2

**Media:** standard 35-track, 18-sector, 256-byte-sector `.DSK` images
(161,280-byte payload). Put them **in any folder on the card**. Use `.DSK` filenames
for the CP400 picker; other machines' DSK files are not interchangeable.

1. Prepare the BASIC and disk-controller ROMs from the checklist.
2. Open **F12 > Boot Disk ROM** and choose **CP400** or **CoCo 2**, according
   to the disk software. After changing it, choose **Reboot CP400** to
   start with that controller ROM.
3. Open **F12 > Disk drives**, select **Drive 0**, press Enter and choose
   the game's DSK. Drives **0-3** are available for multi-disk software.
   Enter opens a folder or selects an image; `..` or Esc returns to the
   parent folder, and Esc at the root cancels. PgUp/PgDn moves a page.
   The selected image's contents are previewed below the list.
4. Press Esc back to the main menu, then Esc or **Exit menu** to return to
   BASIC. Assigning a disk does not automatically run it.
5. List the directory:

   ```basic
   DIR
   ```

6. Follow the game's instructions. A BASIC program commonly uses:

   ```basic
   LOAD "GAME"
   RUN
   ```

   A machine-code program commonly uses:

   ```basic
   LOADM "GAME"
   EXEC
   ```

Some software needs a particular loader, disk ROM, drive number or entry
address. Use its original instructions instead of trying every command.
For disk changes, reassign the requested virtual drive through F12;
Delete ejects its image. Disk assignments are saved on SD with their full
paths (up to 255 characters, including the leading `/`).

**Saving:** disk writes can modify the original DSK on microSD. Keep a
backup and allow writes to finish before resetting or powering off.
This is a floppy-image workflow; do not assume cassette or CoCo 3 support.

### MSX / MSX2 / MSX2+

Select a BIOS profile appropriate to the game's required MSX generation
and RAM. Machine/profile changes require a cold boot.

#### Cartridge games

1. Put the extracted game `.rom` in a folder such as `msx\roms`.
2. Open **F12 > Media**, select **Slot 1**, and choose the cartridge.
   A second cartridge can be selected in Slot 2.
3. Leave the mapper on **Auto** initially. If the game requires a supported
   manual mapper, change the Mapper row below that slot.
4. Choose **Reboot and save configuration** to make this the next boot
   default, or **Reboot without saving** for this session only.
5. The cartridge starts through the selected BIOS. Simply closing F12
   would have resumed the old cartridge configuration.

Sound hardware is a separate **Media > Audio** setting, not a cartridge
mapper. PSG/SCC/FM requirements vary. FM-PAC/MSX-MUSIC modes need a
compatible user-supplied audio BIOS, normally `msx\audio\FMPAC.ROM`.
See the [MSX audio reference](../software/esp32_msx_emulator/README.md#audio-profiles-and-fm-pac--msx-music).
Unsupported special mappers are not made compatible by renaming the ROM.

#### Disk games

1. Supply a compatible disk BIOS as **`DISK.ROM` in the selected BIOS
   profile folder**, then cold-boot. The documented tested option is the
   unmodified 16 KiB Philips NMS8250 disk BIOS. A game DSK is not a disk BIOS.
2. Use raw **360 KiB or 720 KiB** images: 80 tracks, nine 512-byte sectors,
   one or two sides. A 40-track double-sided 360 KiB image is not supported.
3. Open **F12 > Media > Drive A** and select the game DSK. Use Drive B
   for a second image if the software expects it.
4. For a bootable game, choose a cold-boot/reboot option with the disk attached.
   For BASIC software, resume and list/load the named program, for example:

   ```basic
   FILES "A:"
   LOAD "A:GAME.BAS"
   RUN
   ```

5. For a disk-change prompt, open F12, replace the requested drive's image
   and resume without resetting.

Disks are **read/write**, directly modifying their DSK files. Use different
images for A/B and keep backups. The original Panasonic disk ROMs are not
substitutes for the supported interface's compatible disk BIOS.

#### Cassette games

1. Open **F12 > Media > Tape** and attach a standard MSX `.cas` image.
2. Resume BASIC and use the software's loading command. Common examples:

   ```basic
   CLOAD
   RUN
   ```

   or `LOAD "CAS:"` followed by `RUN`; binary tapes typically use
   `BLOAD "CAS:",R`.
3. Use **Rewind tape** before starting again after the tape has advanced.

CAS is read-only. WAV recordings are not supported. Selecting a tape does
not type the loading command or reset the computer.

### MSX TurboR / FS-A1ST (experimental)

This is a **separate firmware**, not another ordinary-MSX BIOS profile.
Use its full FS-A1ST firmware/Kanji files from the checklist.
Performance and physical-board behavior remain under validation; do not
assume full-speed TurboR emulation.

#### Cartridge games

1. Put a supported `.rom` under `msxturbor\roms`, or use a compatible ROM
   elsewhere on the card.
2. Open **F12 > Media**, select **Slot 1** or **Slot 2**, and choose the file.
3. Start with **Auto** mapping. TurboR uses its own mapper detection,
   not the ordinary MSX firmware's mapper database.
4. Select **Reboot and save configuration** or **Reboot without saving**.

Supported cartridges have an `AB` header, are multiples of 8 KiB, and are
8 KiB to 2 MiB. Manual options include common Konami/ASCII mappers,
GameMaster2 and FM-PAC; unsupported special hardware remains unsupported.

#### Disk games

1. Put raw **360/720 KiB** DSK images under `msxturbor\disks`.
2. Keep **Built-in firmware: Off** for normal BASIC/DOS startup.
   That option controls the original Panasonic built-in software switch,
   **not** which ESP32 emulator is installed.
3. Open **F12 > Media > Drive A** and select the game disk; optionally
   assign Drive B.
4. Cold-boot to launch bootable software. For BASIC disk programs, use
   the software's normal MSX loading commands as above.
5. Swap disks live through F12 when requested; do not cold-boot just to
   satisfy a disk-change prompt.

The full A1ST firmware provides its disk interface; no separate ordinary-MSX
`DISK.ROM` is needed. Writes go directly to the DSK, so back it up. Do not
attach the same writable image to both drives.

**Important differences:** CAS/tape is unsupported; use ordinary MSX for
cassettes. The current board integration has no working USB joystick
input, so use keyboard-controlled software. Punctuation follows the
Japanese machine's keyboard matrix. Enter F12 before power-off to flush
SRAM/CMOS, including supported cartridge SRAM.

### TK90X / TK95

Choose TK95 or **48 KiB TK90X** and its matching ROM in
**F12 > Configuration / ROM**, then cold-boot.

#### TAP cassette games

1. Put a standard `.tap` in a folder such as `tk\tapes`.
2. Open **F12 > Media** and attach it.
3. Return to BASIC and enter:

   ```basic
   LOAD ""
   ```

   This uses the original TK/Spectrum keyboard matrix: on the normal
   keyword-entry ROM, **J** enters `LOAD`. For the two double quotes,
   use the ROM's SYMBOL SHIFT + P chord twice; USB **Ctrl or Alt** acts
   as SYMBOL SHIFT. The PC Shift key acts as CAPS SHIFT.
4. Open **F12 > Media**, select **Play tape**, then resume.
5. Wait for loading. Tape runs at real **emulated** speed, not instant
   loading; a slow-running emulation also makes it take longer in real time.

Use **Stop/Rewind** to retry, then reissue the loading command and play.
Attachment alone does not play. F12 pauses tape position; warm/cold reset
stops and rewinds it. Only the tape path, not its position, is remembered.

#### Snapshot games

1. Boot the desired TK ROM.
2. Open **F12 > Media** and select the snapshot-loading action.
3. Choose a supported **48K `.sna` or `.z80`** snapshot, then resume.

SNA must be the 49,179-byte 48K form; Z80 v1 and plain-48K v2/v3 variants
are supported. A snapshot restores software state, not a ROM/model or
joystick configuration.

**Limits:** no TZX/WAV, 128K snapshots, recording, snapshot saving or
certified turbo/protected tape loading. TAP images are read-only and
limited to 4 MiB. Select the joystick interface expected by the game.

### Apple II+ / original Apple IIe

Prepare the selected model ROM and the slot-6 Disk II ROM. Enhanced
IIe/65C02 software is not supported just because it uses an IIe filename.

1. Put the game disk under `apple2\disks` as a **35-track `.nib` image**,
   exactly **232,960 bytes**.
2. If needed, choose the machine and ROMs in **F12 > Configuration / ROM**.
3. Open **F12 > Media / Disk II drives**, select **Drive 1**, and choose
   the boot disk. Assign Drive 2 if required by the software.
4. Choose **Reboot and save configuration** or **Reboot without saving**,
   or return to the main menu and select **Cold boot**.
5. Wait for Disk II to load. From an already running BASIC prompt, `PR#6`
   also boots the disk in drive 1.

Inserting a boot disk alone does not restart the CPU. If you booted
without media and the Disk II ROM is waiting, **F12 > Warm reset** can
take you into BASIC; mount a disk and cold-boot when ready.

For a multi-disk game's swap prompt, replace the specified drive image in
F12 and resume without resetting.

**Limits:** both drives are **read-only**. DSK/DO/PO/WOZ files are not
accepted; renaming them to NIB does not convert them. There is no disk
save, cassette or snapshot support. Loading takes emulated disk time and
may be slower than the original computer.

Useful keyboard translations include **Left Alt = Open Apple**,
**Right Alt = Solid Apple**, and **Enter = Return**. Use the game's own
instructions for keyboard or paddle controls.

### Macintosh Plus

Macintosh software is launched from a booted Macintosh operating system,
not by selecting an individual game ROM.

#### Prepare a bootable disk on a PC

1. Supply the unmodified 128 KiB Macintosh Plus ROM.
2. Prepare a **raw whole SCSI disk image** in a compatible emulator such
   as MAME, with its partition map, Macintosh SCSI driver, bootable
   compatible system, and your applications/games already installed.
3. Copy that image to `macplus\disks\hd.img`, or another selectable IMG
   path. Keep a backup.

A floppy image or a bare HFS volume used by some other emulators is **not**
the required whole-SCSI-disk image. A newly created blank image does not
contain a system and will not boot.

#### Boot and play

1. Connect a compatible low-speed boot mouse to the **middle USB-A socket**.
   Keep the keyboard in the rightmost socket.
2. In F12, select the ROM and whole-disk `.img`; choose read-only access
   for the next boot if desired. Save the boot configuration if wanted.
3. Select **Cold boot**. The disk appears at **SCSI ID 6**.
4. Once the desktop appears, open the disk/folder and double-click the
   installed game's application.
5. To add more software, finish saves/shutdown, turn the computer off,
   back up the IMG, and modify it in the compatible PC emulator. Safely
   return the updated image to the card before booting again.

ROM, disk and protection changes apply on **cold boot**, not on resume.
The active system disk is not hot-swapped. Read/write is the default and
changes persist in the IMG; read-only mode prevents saves and may not suit
software that expects a writable system disk.

**Limits:** images must be 512-byte aligned and no larger than 1 GiB.
There is no floppy insertion, networking or save-state support.
GUI/Windows keys map to **Command**, Alt to **Option**; F12 stays the host
menu key.

### Sega Master System

No BIOS is required.

1. Put an extracted `.sms` or `.rom` game under `sms\roms`.
2. Open **F12 > Load ROM** and select it. Confirm replacement if another
   game is already running.
3. A successful load starts the game immediately.
4. Use **Save boot configuration** if you want that game/settings at the
   next startup; loading alone does not change saved defaults.

| Keyboard control | Action |
| --- | --- |
| Arrows | Player 1 directions. |
| Z or Space / X | Player 1 button 1 / button 2. |
| W / S / A / D | Player 2 directions. |
| G / H | Player 2 button 1 / button 2. |
| Enter or Pause | SMS console Pause, with game-dependent behavior. |
| R | Console Reset line; may restart the game. |
| F12 | Host menu and reliable emulator pause. |

Calibrated joystick ports 1/2 control players 1/2.
Start with the mapper on **Auto**; manual Sega/Codemasters/Korean choices
are available. Timing/region/mapper changes require **Configuration >
Reboot selected cartridge** or a new ROM load; ordinary reset keeps the
running configuration.

**Limits:** supported payloads are 8 KiB or multiples of 16 KiB up to
1 MiB; an optional 512-byte copier header is removed automatically.
No ZIP loading, Game Gear/SG-1000 mode or save states.
**Cartridge SRAM lasts only for the session**: it is not saved to SD and
may be lost on reset, game replacement, firmware switch or power-off.

### Commodore 64

Prepare all three boot ROMs, start the core and **wait for `READY.`**
before loading software from F12.

#### PRG files

1. Put the game `.prg` under `c64\programs`.
2. Open **F12 > Media / PRG and D64 > Load PRG into current machine**.
3. Choose the file. The loader copies it to the load address in its header.
4. Resume and type `RUN`, or choose **Queue BASIC RUN and resume** in Media.
5. For machine-code programs without a BASIC starter, use the program's
   documented `SYS` address instead; `RUN` is not universal.

#### Programs inside D64 images

1. Open **F12 > Media / PRG and D64 > Load a PRG from D64**.
2. Select a supported 35-track D64 and then the desired closed PRG entry.
3. Resume and use `RUN`, the queued RUN action, or the documented `SYS`
   command as appropriate.

**D64 is read-only program extraction, not an emulated floppy drive.**
Do not use `LOAD "*",8,1` expecting an attached 1541. Games that fetch
additional files after starting generally need a multi-load/drive
implementation that this firmware does not provide.

Supported D64 sizes are **174,848 bytes**, or **175,531 bytes** with an
error table. No disk writes/`SAVE`, tapes, CRT cartridges or snapshots
are implemented. Load at the BASIC prompt, not over an actively running
game; reset first when changing programs.

Many games require joystick **port 2**. Use the middle socket, or enable
**Swap joystick ports** when using the leftmost socket.
**Esc = RUN/STOP**, **F11 = RESTORE**, **Alt/GUI = Commodore key**.

## Firmware switching and updates

### Prepare once, then switch through F12

The F12 updater installs another emulator's **FLH application package**.
It is not a launcher that keeps all eight firmwares installed and running.
You can keep all the **packages and SD assets** together on one card.

1. Obtain a current, trusted package built specifically for this board.
   If building from source, each firmware's `make firmware` command
   produces its package in that project's `dist` directory.
2. With the computer off, copy the destination system's required ROMs
   and media to the card.
3. Copy its package to the **card root**, retaining the filename:

   | Destination | Package filename pattern |
   | --- | --- |
   | CP400 / CoCo 2 | `ESP32_CP400-<version>.FLH` |
   | MSX | `ESP32_MSX-<version>.FLH` |
   | MSX TurboR | `ESP32_MSXTURBOR-<version>.FLH` |
   | TK90X / TK95 | `ESP32_TK95-<version>.FLH` |
   | Apple II | `ESP32_APPLE2-<version>.FLH` |
   | Macintosh Plus | `ESP32_MACPLUS-<version>.FLH` |
   | Sega Master System | `ESP32_SMS-<version>.FLH` |
   | Commodore 64 | `ESP32_C64-<version>.FLH` |

4. Safely eject the card from the PC, insert it and start the computer.
5. Open **F12 > Firmware update**. TurboR calls it **Firmware update /
   switch emulator**.
6. Select the destination FLH, check the name and confirm as prompted.
7. **Do not turn off, reset or remove the card during installation.**
   Wait for validation, writing and the automatic restart.
8. The destination firmware starts. Its ROMs, input settings and boot
   media must still be configured as required.

To return to another system, repeat with that system's FLH from the new
firmware's F12 menu. **No renaming is needed with current updaters.**
The source FLH remains on the card.

Installing a firmware is not a save-state operation: running software
does not survive the restart. Save supported game data first.

### Older firmware and recovery

- Switching requires the **shared dual 4 MiB OTA partition layout**.
  A first installation uses a UART upload to install the bootloader,
  partition table and application.
- Older MSX/TK/Apple updaters that reject other emulator filenames must
  first update to a current build of **their own** firmware.
- Older CP400 installations with the smaller partition layout require
  a **one-time UART upload** of a current build. FLH cannot change the
  bootloader or partition table.
- Use current destination builds so switching remains available after
  reboot. If an updater rejects a package, record its error and check
  package integrity/layout rather than bypassing the check.

Package checksums detect corruption, **not authenticity**. Install only
trusted firmware for this hardware, not an arbitrary ESP32-S3 binary.
Cross-firmware mock-OTA checks cover all eight builds; they do not certify
every real-board transition or guarantee recovery from power loss.
See the relevant [setup reference](#firmware-specific-references) for UART
maintenance rather than experimenting with the programming/boot controls.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| No picture | Left power button on; supplied 5 V lead connected; monitor on the VGA input; VGA cable seated. Test direct VGA before introducing a converter. |
| Picture stretched or shifted | Use 4:3/Aspect and monitor auto-adjust. For TurboR clipping, try F12 **VGA fit**. |
| Keyboard or F12 does nothing | Keyboard in the **rightmost** socket, connected directly; try Fn + F12 or a basic wired HID keyboard. |
| Joystick not detected | Confirm low-speed HID compatibility, direct connection and supported report format. Calibration cannot fix full-speed-only hardware. |
| Joystick detected but game ignores it | Calibrate in this firmware, enable the port where applicable, and select the game's port/interface. Check C64 port 2 and TK Kempston/Sinclair. TurboR currently has no USB joystick input. |
| Macintosh mouse does not work | Use a low-speed HID boot mouse in the **middle** socket, not a joystick in that socket. |
| SD/media not found | FAT32; correct root folders and filenames; actual supported, extracted image. Browse to CP400 DSK files with **F12 > Disk drives**. |
| Missing BIOS/ROM error | Check the selected profile's required raw files, sizes and paths. A correctly sized file may still be the wrong ROM. |
| File selected but nothing starts | Check whether the core needs cold boot, a BASIC command, TAP **Play tape**, or a C64 `RUN`/`SYS`. |
| Apple waits with no disk | Insert a boot NIB in drive 1 and cold-boot, or use Warm reset to reach BASIC. Loading can take emulated disk time. |
| Mac shows missing boot media | Supply a bootable **whole SCSI disk**, not a floppy, bare HFS volume or blank IMG. |
| No sound | Use the separate audio output and powered speakers; check their volume and the core's sound setting. F12 pauses/mutes audio in most cores; MSX FM may require an audio BIOS. |
| Settings disappear after restart | Use that firmware's explicit save-configuration action. Saving settings does not save game state or session-only SRAM. |
| Game runs slowly or fails | Check required machine/RAM/mapper and supported media. Try rendering fewer frames if offered; this reduces drawing work, not compatibility requirements. Some cores are early-stage. |
| Firmware package rejected | Use a current trusted FLH for this board. Check the reported error and older-updater/partition requirements above. |

If the card failed to mount initially, use **Retry microSD** or **Rescan SD**
where offered. If physically replacing an already mounted card, **power
off first and restart afterward**, rather than relying on rescan.
Back up/recover images on a PC after a write error. Advanced diagnostics
are available through the UART console.

## Firmware-specific references

These developer/setup guides provide import tools, exact ROM requirements,
build/upload commands and detailed implementation limits:

- [CP400 / CoCo 2](../software/esp32_cp400_emulator/README.md)
- [MSX / fMSX](../software/esp32_msx_emulator/README.md)
- [MSX TurboR / FS-A1ST](../software/esp32_msxturbor_emulator/README.md)
- [TK90X / TK95](../software/esp32_tk95_emulator/README.md)
- [Apple II+ / original Apple IIe](../software/esp32_apple2_emulator/README.md)
- [Macintosh Plus](../software/esp32_macplus_emulator/README.md)
- [Sega Master System](../software/esp32_sms_emulator/README.md)
- [Commodore 64](../software/esp32_c64_emulator/README.md)
