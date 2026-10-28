WINDOWS_USERPROFILE := $(subst \,/,$(USERPROFILE))
PIO ?= $(WINDOWS_USERPROFILE)/.platformio/penv/Scripts/pio.exe
FW_VERSION := 1.04
PIO_ENV := esp32-s3-devkitc-1-n16r8

.PHONY: all firmware build upload test clean help
all: firmware
build:
	"$(PIO)" run -e "$(PIO_ENV)"
upload:
	"$(PIO)" run -e "$(PIO_ENV)" -t upload
firmware: build
	powershell -NoProfile -ExecutionPolicy Bypass -File "tools\Package-Firmware.ps1" -Version "$(FW_VERSION)"
test:
	powershell -NoProfile -ExecutionPolicy Bypass -File "tools\Test-Core.ps1"
	powershell -NoProfile -ExecutionPolicy Bypass -File "tools\Test-Audio.ps1"
	powershell -NoProfile -ExecutionPolicy Bypass -File "tools\Test-SoundMenu.ps1"
	powershell -NoProfile -ExecutionPolicy Bypass -File "tools\Test-Frontend.ps1"
	powershell -NoProfile -ExecutionPolicy Bypass -File "tools\Test-FileBrowser.ps1"
	powershell -NoProfile -ExecutionPolicy Bypass -File "tools\Test-RomImport.ps1"
	powershell -NoProfile -ExecutionPolicy Bypass -File "tools\Test-FirmwareUpdate.ps1"
	powershell -NoProfile -ExecutionPolicy Bypass -File "tools\Test-Joysticks.ps1"
	powershell -NoProfile -ExecutionPolicy Bypass -File "tools\Test-JoystickRuntime.ps1"
	powershell -NoProfile -ExecutionPolicy Bypass -File "tools\Test-JoystickTiming.ps1"
	powershell -NoProfile -ExecutionPolicy Bypass -File "tools\Test-JoystickFrontend.ps1"
clean:
	"$(PIO)" run -e "$(PIO_ENV)" -t clean
help:
	@echo make firmware - Build and verify dist\ESP32_MSXTURBOR-$(FW_VERSION).FLH
	@echo make upload - Install shared OTA layout and firmware through UART
	@echo make test - Run host core, frontend, importer and updater regressions
