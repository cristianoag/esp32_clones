WINDOWS_USERPROFILE := $(subst \,/,$(USERPROFILE))
PLATFORMIO_VENV := $(WINDOWS_USERPROFILE)/.platformio/penv/Scripts
ifneq ($(wildcard $(PLATFORMIO_VENV)/pio.exe),)
PIO ?= $(PLATFORMIO_VENV)/pio.exe
else
PIO ?= pio
endif

FW_VERSION := 1.01
PIO_ENV := esp32-s3-devkitc-1-n16r8
OUTPUT_DIR ?= dist
UPDATE_FILE ?= $(OUTPUT_DIR)/ESP32_MACPLUS-$(FW_VERSION).FLH
RAW_FIRMWARE := .pio/build/$(PIO_ENV)/firmware.bin

.PHONY: all firmware build upload package verify test clean help
all: firmware
firmware: verify
build:
	"$(PIO)" run -e "$(PIO_ENV)"
upload:
	"$(PIO)" run -e "$(PIO_ENV)" -t upload
package: build
	powershell -NoProfile -ExecutionPolicy Bypass -File "tools\Package-Firmware.ps1" -Source "$(RAW_FIRMWARE)" -Output "$(UPDATE_FILE)"
verify: package
	powershell -NoProfile -ExecutionPolicy Bypass -File "tools\Package-Firmware.ps1" -Source "$(RAW_FIRMWARE)" -Output "$(UPDATE_FILE)" -VerifyOnly
test:
	powershell -NoProfile -ExecutionPolicy Bypass -File tools\Test-Core.ps1
	powershell -NoProfile -ExecutionPolicy Bypass -File tools\Test-JoystickTiming.ps1
	powershell -NoProfile -ExecutionPolicy Bypass -File tools\Test-FirmwareUpdate.ps1
	powershell -NoProfile -ExecutionPolicy Bypass -File tools\Test-ImportSdCard.ps1
clean:
	"$(PIO)" run -e "$(PIO_ENV)" -t clean
help:
	@echo make firmware - Build, package and verify ESP32_MACPLUS-$(FW_VERSION).FLH
	@echo make upload   - Upload using the board UART connector
	@echo make test     - Run native Mac Plus and updater regression tests
