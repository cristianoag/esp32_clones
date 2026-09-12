WINDOWS_USERPROFILE := $(subst \,/,$(USERPROFILE))
PLATFORMIO_VENV := $(WINDOWS_USERPROFILE)/.platformio/penv/Scripts
ifneq ($(wildcard $(PLATFORMIO_VENV)/pio.exe),)
PIO ?= $(PLATFORMIO_VENV)/pio.exe
else
PIO ?= pio
endif

FW_VERSION := 1.01
PIO_ENV := esp32-s3-devkitc-1-n16r8
ifneq ($(wildcard $(PLATFORMIO_VENV)/python.exe),)
PYTHON ?= $(PLATFORMIO_VENV)/python.exe
else
PYTHON ?= python
endif
OUTPUT_DIR ?= dist
UPDATE_FILE ?= $(OUTPUT_DIR)/ESP32_MSX-$(FW_VERSION).FLH
RAW_FIRMWARE := .pio/build/$(PIO_ENV)/firmware.bin

.PHONY: all firmware build upload package verify clean test help
all: firmware
firmware: verify
build:
	"$(PIO)" run -e "$(PIO_ENV)"
upload:
	"$(PIO)" run -e "$(PIO_ENV)" -t upload
package: build
	"$(PYTHON)" -c "from pathlib import Path; source = Path(r'$(RAW_FIRMWARE)'); output = Path(r'$(UPDATE_FILE)'); firmware = source.read_bytes(); assert firmware[:1] == b'\xe9', 'Payload is not an ESP32 application image'; body = b'-~' + firmware; output.parent.mkdir(parents=True, exist_ok=True); output.write_bytes(str(sum(body)).encode('ascii') + body); print(f'Created {output} ({output.stat().st_size} bytes)')"
verify: package
	"$(PYTHON)" -c "from pathlib import Path; package = Path(r'$(UPDATE_FILE)').read_bytes(); checksum, payload = package.split(b'-', 1); assert checksum.isdigit(), 'Invalid checksum header'; assert payload[:1] == b'~', 'Missing firmware separator'; assert int(checksum) == sum(b'-' + payload), 'Checksum verification failed'; assert payload[1:2] == b'\xe9', 'Payload is not an ESP32 application image'; assert payload[1:] == Path(r'$(RAW_FIRMWARE)').read_bytes(), 'Payload differs from built firmware'; print('Firmware package verified: $(UPDATE_FILE)')"
clean:
	"$(PIO)" run -e "$(PIO_ENV)" -t clean
	"$(PYTHON)" -c "from pathlib import Path; Path(r'$(UPDATE_FILE)').unlink(missing_ok=True)"
test:
	powershell -NoProfile -ExecutionPolicy Bypass -File tools\Test-RomImport.ps1
	powershell -NoProfile -ExecutionPolicy Bypass -File tools\Test-Profiles.ps1
	powershell -NoProfile -ExecutionPolicy Bypass -File tools\Test-Settings.ps1
	powershell -NoProfile -ExecutionPolicy Bypass -File tools\Test-FirmwareUpdate.ps1
	powershell -NoProfile -ExecutionPolicy Bypass -File tools\Test-Joysticks.ps1
	powershell -NoProfile -ExecutionPolicy Bypass -File tools\Test-JoystickRuntime.ps1
	powershell -NoProfile -ExecutionPolicy Bypass -File tools\Test-JoystickTiming.ps1
	powershell -NoProfile -ExecutionPolicy Bypass -File tools\Test-Audio.ps1
	powershell -NoProfile -ExecutionPolicy Bypass -File lib\fmsx\tests\media.ps1
help:
	@echo make firmware - Build, package and verify the FLH file
	@echo make build    - Build the raw firmware only
	@echo make upload   - Build and upload through the PC USB port
	@echo make clean    - Clean build output and the selected FLH file
	@echo Firmware version: $(FW_VERSION)
	@echo Output: $(UPDATE_FILE)
