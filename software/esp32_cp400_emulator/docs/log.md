# Firmware Change Log

This log tracks user-visible changes to the ESP32 CP400 emulator firmware.
Versions use one major digit and two minor digits, for example 1.10 and 1.11.

## 1.16 - Unreleased

### Fixed

- Centered every F12 menu screen within the active VGA frame, including text, selection highlights, disk previews and firmware progress bars, without moving the emulated computer display.
- Restored the complete emulated display in both VGA buffers when closing F12, including pixels beyond the first 320 columns in wider video modes.

### Added

- Added host regression checks for balanced menu margins, text clipping and complete display restoration in all four VGA modes.

## 1.15 - Unreleased

### Changed

- Changed the parent-folder `make` and `make firmware` commands to clear FLH files from the shared SD-card root and copy only the latest CP400 package there, leaving emulator project roots and build archives unchanged.

### Fixed

- Allowed Disk drives to browse folders and parent folders anywhere on the microSD card, preview nested DSK images and save their complete paths without truncation.
- Corrected disk-image paging so folders and unrelated files no longer caused images to be skipped or repeated.
- Paused the CPU and software-USB interrupt before reading OTA metadata, not just before writing firmware, preventing the cache-disabled crash that rebooted back into CP400 after Y confirmation.
- Kept that interrupt paused through OTA handle cleanup and restored it after failed layout checks, SD reads and image installation.
- Stopped routing the VGA pixel clock onto GPIO48, which could light the RGB LED again after startup had cleared it.
- Started native keyboard initialization before the software-USB CPU timer and cleared held menu keys when the keyboard disconnected.
- Stopped configuring BOOT GPIO0 as an unused software-USB activity output on ESP32-S3.
- Configured the UART uploader and monitor for COM9 and stopped the monitor from asserting DTR and RTS when opening the port.
- Kept firmware installation errors visible until ENTER or ESC acknowledged them and wrapped long messages so recovery instructions were not clipped.
- Reported firmware selection, Y confirmation, the installed partition layout and the selected boot slot on UART so skipped or rejected updates could be diagnosed.
- Distinguished an incompatible partition table from an already selected boot update, with the appropriate recovery instructions for each.
- Accepted Y after the firmware picker released ENTER without discarding a confirmation pressed while its screen was being drawn.

## 1.14 - Unreleased

### Added

- Copied the latest prebuilt CP400 firmware package to this emulator's project root and the shared SD-card root when running `make` or `make firmware` from the parent software folder, without rebuilding firmware.

### Fixed

- Allowed Firmware update to browse folders and parent folders anywhere on the microSD card instead of listing only files at the root.
- Preserved the complete selected path when installing another emulator's FLH package and corrected paging so unrelated files did not hide firmware choices.
- Displayed the selected firmware name before confirmation and retained checksum, ESP32-S3 image and inactive-slot validation for all eight Clone Series emulators.

## 1.13 - Unreleased

### Changed

- Allowed Firmware update to install compatible CP400, MSX, TK95 and Apple II FLH packages without renaming, preserving the source file on SD.
- Replaced reboot-time staging with validated inactive-slot installation, progress and explicit errors before restarting into the selected emulator.
- Paused CPU and software-joystick interrupts during installation and reserved additional menu stack space for buffered OTA updates.
- Adopted the shared dual 4 MiB OTA layout, requiring one UART upload for boards using the older CP400 partition table.
- Added a setup guide and firmware-update host tests covering cross-emulator packages and failure handling.
- Loaded the CP400 BASIC, Extended BASIC, and selected CP400 or CoCo disk controller ROM from `cp400/bios` on the microSD card instead of embedding them in the firmware. Startup now stops with a serial error when a required ROM is missing, unreadable, or the wrong size.
- Drove the sound output on GPIO47 only, matching the board revision that removes the second audio channel. On this module GPIO48 is the data line of the onboard RGB LED, so audio could not share it. The machine's sound is mono, so nothing is lost.
- Raised the audio carrier well above the audible range, so the board's audio filter now removes almost all of it instead of leaving an ultrasonic tone on the jack.
- Moved the emulated ROM and RAM from the external PSRAM chip into the ESP32's internal memory, which is considerably faster for the emulated processor.

### Fixed

- Tracked USB disconnection separately for each joystick connector so activity on one port no longer determined the other port's connection status.
- Removed an empty VGA library manifest so PlatformIO used the existing Arduino library metadata without reporting manifest parsing errors.
- Centered the 256x192 CP400 display area within the active VGA frame in every video mode. Graphics and text modes now share the same origin calculated from the selected VGA resolution instead of using separate manual offsets.
- Turned the onboard RGB LED off at startup. It sits on GPIO48, which the first version of this board also used as a second audio channel, so the sound signal reached the LED as if it were colour data and left it lit. The LED holds the last colour it is given until it is told otherwise, so it is now explicitly cleared when the emulator starts.
- Honoured the sound multiplexer instead of treating it as permanently open, so port writes that were never meant to be audible, such as serial framing and joystick reads, no longer reach the speaker.
- Ignored the two lowest bits of the sound port, which carry the serial and cassette lines rather than sound data and added noise to every sample.
- Stopped the sound output switching once the machine stops producing sound. Holding a level required the output to keep switching, which left a faint carrier on the jack when the machine was silent.

## 1.12 - 2026-08-29

### Changed

- Moved the keyboard port to the ESP32-S3 native USB-OTG host on GPIO19 and GPIO20, so ordinary full speed USB keyboards now work. The previous bit-banged stack could only talk to low speed devices, which is why most keyboards appeared dead or never lit their indicators.
- Requested the HID boot protocol from the keyboard, so keyboards that default to their own report layout still deliver the standard report the emulator expects.
- Turned the native USB serial console off, because the keyboard host and the console cannot share the one USB controller. Serial output and firmware uploads now use the UART port, which already carried the boot log.
- Kept joystick 1 and joystick 2 on the existing bit-banged stack, unchanged.

### Added

- Logged the keyboard's speed, vendor and product IDs, and the interface and endpoint in use, so an unsupported keyboard can be told apart from a power or wiring fault.
- Added `make upload` to build and upload the firmware through the PC USB port configured in `platformio.ini`.
- Added a DELETE action to eject the selected disk image from any of the four drives and persist the empty assignment.

### Fixed

- Removed UART debug writes from the 6809 timer interrupt. They caused an interrupt watchdog reset loop after the USB-OTG keyboard host moved the serial console from native USB CDC to UART0.
- Showed explicit Y/Yes and N/No choices on the firmware installation confirmation screen.
- Waited for ENTER to be released before opening the disk-drive screen, preventing the same key press from immediately opening Drive 0's file picker.

### Notes

- The keyboard connector must be rewired: D- goes to GPIO19 and D+ goes to GPIO20. These pads are tied to the USB PHY inside the chip and cannot be moved to other pins.
- Keyboards containing an internal USB hub are still not supported, because this ESP-IDF version has no hub driver.

## 1.11 - 2026-08-19

### Added

- Added a progress gauge that runs while a firmware update is validated and copied.

### Changed

- Moved the USB keyboard port to GPIO11 and GPIO12, joystick 1 to GPIO15 and GPIO16, and joystick 2 to GPIO17 and GPIO18, to simplify cable routing on the board.
- Gave every F12 screen the same layout, with the product name and screen title in the header, matching separator lines, and key hints in the footer. The disk image picker, firmware update screens, and error messages previously used their own styles.
- Renamed the "Joystick calibration" menu entry to "Joystick setup".
- Turned the joystick Serial debug output off by default.
- Listed only files with a matching extension when choosing a firmware or disk image, hiding folders such as System Volume Information that could not be opened anyway.

## 1.10 - 2026-08-18

### Added

- Added independent joystick 1 and joystick 2 control mapping from the F12 menu.
- Added guided capture for joystick neutral, up, down, left, right, button 1, and button 2 states.
- Added persistent raw HID joystick mappings to support controllers with different report layouts.
- Added a Serial joystick debug mode in the F12 menu that logs raw USB reports, decoded controls, and stored mappings at 115200 baud.
- Added a Makefile workflow that builds, packages, and verifies `.FLH` firmware updates for the F12 menu.
- Added firmware version control through the Makefile and versioned update-package filenames.

### Fixed

- Stored the PIA control registers at $FF01 and $FF03 on write, so the joystick multiplexer select bits change as the machine requests. They were never updated, which froze the multiplexer on a single channel and made every axis read return the same value.
- Restored the CP400 PIA joystick multiplexer order, which had been changed incorrectly and routed joystick 1's horizontal axis to the vertical channel while the horizontal channel read the unused second joystick.
- Centred the default joystick axis values, so a disconnected or silent joystick no longer reads as held fully left and up.
- Prevented an empty comparison mask from matching every direction at once, which cancelled horizontal and vertical movement and left the pointer centred.
- Moved joystick tracing to a dedicated task on the free core, so the video task cannot starve it and Serial output cannot disturb the bit-banged USB timing.
- Routed joystick tracing to the UART0 console used by the rest of the boot output, instead of the separate native USB serial port where it was invisible.
- Mirrored a single connected joystick's axes onto both CP400 joystick channels, so games work regardless of which joystick they read or which USB port the controller uses.
- Logged USB device enumeration with port number and vendor/product IDs, and the stored joystick mappings at boot.
- Replaced whole-report comparison with per-control bit masks so hat switches, buttons, and analog axes are matched independently.
- Excluded report bytes that change while the joystick is at rest, so counters and noisy axes no longer block button detection.
- Mapped either physical joystick button to the CP400's single fire input for that joystick.
- Added automatic detection of the standard Windows PlatformIO CLI installation.
- Corrected `.FLH` package checksums to match the F12 firmware validator.
- Fixed F12 firmware staging so stale or partial update files are replaced and verified before rebooting.

## 1.00 - 2026-08-17

### Added

- First tracked firmware release for the ESP32 CP400 emulator.
- Added CP400/CoCo 2 emulation with VGA output, USB keyboard and joystick input, SD-backed disk images, the F12 system menu, and SD-card firmware updates.