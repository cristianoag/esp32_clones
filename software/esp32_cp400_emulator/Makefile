WINDOWS_USERPROFILE := $(subst \,/,$(USERPROFILE))
PLATFORMIO_VENV := $(WINDOWS_USERPROFILE)/.platformio/penv/Scripts

ifneq ($(wildcard $(PLATFORMIO_VENV)/pio.exe),)
PIO ?= $(PLATFORMIO_VENV)/pio.exe
else
PIO ?= pio
endif

PYTHON ?= python
PIO_ENV ?= esp32-s3-devkitc-1-Modded
FW_VERSION ?= 1.13
OUTPUT_DIR ?= dist
UPDATE_FILE ?= $(OUTPUT_DIR)/ESP32_CP400-$(FW_VERSION).FLH

export FW_VERSION

RAW_FIRMWARE := .pio/build/$(PIO_ENV)/firmware.bin

.PHONY: all firmware build upload package verify clean help

all: firmware

firmware: verify

build:
	$(PIO) run -e "$(PIO_ENV)"

upload:
	$(PIO) run -e "$(PIO_ENV)" -t upload

package: build
	$(PYTHON) -c "from pathlib import Path; source = Path(r'$(RAW_FIRMWARE)'); output = Path(r'$(UPDATE_FILE)'); firmware = source.read_bytes(); body = b'-~' + firmware; output.parent.mkdir(parents=True, exist_ok=True); output.write_bytes(str(sum(body)).encode('ascii') + body); print(f'Created {output} ({output.stat().st_size} bytes)')"

verify: package
	$(PYTHON) -c "from pathlib import Path; package = Path(r'$(UPDATE_FILE)').read_bytes(); checksum, payload = package.split(b'-', 1); assert checksum.isdigit(), 'Invalid checksum header'; assert payload[:1] == b'~', 'Missing firmware separator'; assert int(checksum) == sum(b'-' + payload), 'Checksum verification failed'; assert payload[1:2] == b'\xe9', 'Payload is not an ESP32 application image'; print('Firmware package verified: $(UPDATE_FILE)')"

clean:
	$(PIO) run -e "$(PIO_ENV)" -t clean
	$(PYTHON) -c "from pathlib import Path; Path(r'$(UPDATE_FILE)').unlink(missing_ok=True)"

help:
	@echo make firmware  - Build and create the F12 update package
	@echo make upload    - Build and upload through the configured PC USB port
	@echo make clean     - Clean PlatformIO output and the update package
	@echo PlatformIO: $(PIO)
	@echo Firmware version: $(FW_VERSION)
	@echo Output: $(UPDATE_FILE)