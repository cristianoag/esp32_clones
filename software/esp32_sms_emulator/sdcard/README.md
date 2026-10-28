# SMS SD card layout

Copy this directory's contents to a FAT32 card. Place your legally obtained,
uncompressed `.sms` or `.rom` cartridges in `sms\roms`, creating that directory
if necessary. Empty directories may not survive a Git checkout.

```text
SD root
  sms
    roms
      YourGame.sms
  ESP32_SMS-1.03.FLH       optional firmware update
```

No BIOS is needed. No game images are supplied. The F12 ROM browser can also
navigate other card directories. SRAM and save-state files are not supported.
