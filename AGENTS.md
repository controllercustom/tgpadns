# AGENTS.md — TGPad-NS

## Build & Upload

Build profiles are defined in `sketch.yaml` with local library paths. The default profile is **atoms3**.

### Compile
```bash
# Default target: M5Stack AtomS3 (8MB flash) — verified target
arduino-cli compile .

# Generic ESP32-S3
arduino-cli compile --profile esp32s3 .
```

### Serial upload (board must be in download mode, BOOT button held)
```bash
# AtomS3
arduino-cli upload -p /dev/ttyACM0 --profile atoms3 .

# Generic ESP32-S3
arduino-cli upload -p /dev/ttyUSB0 --profile esp32s3 .
```

### OTA upload (board must have WiFi connected and ArduinoOTA running)
```bash
# AtomS3 — by hostname
arduino-cli compile --output-dir /tmp/tgap_ota_build . && \
  arduino-cli upload -p tgpadns.local --upload-field password="" \
    --protocol network --profile atoms3 --input-dir /tmp/tgap_ota_build .

# Generic ESP32-S3 — verified working
arduino-cli compile --profile esp32s3 --output-dir /tmp/tgap_ota_build . && \
  arduino-cli upload -p <IP> --upload-field password="" \
    --protocol network --profile esp32s3 --input-dir /tmp/tgap_ota_build .
```

Fallback — direct espota.py (bypasses arduino-cli port discovery):
```bash
ESPOTA=~/.arduino15/packages/esp32/hardware/esp32/3.3.10/tools/espota.py
arduino-cli compile --profile esp32s3 --output-dir /tmp/tgap_ota_build . && \
   python3 "$ESPOTA" -r -i <IP> -p 3232 --auth="" \
    -f /tmp/tgap_ota_build/tgpadns.ino.bin; echo "EXIT=$?"
```

Fallback — web-based OTA (no ArduinoOTA needed, HTTP port 80):
```bash
arduino-cli compile --profile esp32s3 --output-dir /tmp/tgap_ota_build . && \
  curl -F "firmware=@/tmp/tgap_ota_build/tgpadns.ino.bin" \
    http://<IP>/update; echo "EXIT=$?"
```

## Critical Gotchas
- **OTA authentication**: Uncomment `#define OTA_PASS "your-password-here"` in `tgpadns.ino` to enable password-protected OTA (both ArduinoOTA and web-based). When enabled, append `--upload-field password=<pass>` to OTA upload commands and use basic auth user `admin` for web OTA.
- **Detecting successful OTA**: Do NOT parse espota.py or curl output — both produce `\r` progress bars that get truncated by tool output limits. Always check exit code (`$?`) instead: 0 = success, non-zero = failure. Append `; echo "EXIT=$?"` to verify.
- **`CDCOnBoot=default`** is mandatory. The old `cdc` value breaks NSLiteController init.
- **Libraries in `lib/` are git submodules**. Profiles handle them via `dir:` entries — no manual `--library` flags needed when using profiles.
- **AtomS3 serial port**: `/dev/ttyACM0` only appears when the board is in download mode (BOOT button held). When running normally it disappears from USB and shows up as a HID gamepad + WiFi device.

## Testing
```bash
pip3 install -r test/requirements.txt

# Offline unit tests (no hardware)
python3 -m pytest test/ -v --ignore=test/e2e

# Hardware e2e — requires root (evdev grab), set board IP via env var
sudo TGAPDNS_HOST=192.168.1.x python3 -m pytest test/e2e -v

# Full e2e with multi-client tests enabled (requires generic ESP32-S3, not AtomS3)
sudo TGAPDNS_HOST=192.168.1.x TGAPDNS_MULTICLIENT=1 python3 -m pytest test/e2e -v
```

- Multi-client e2e tests are **skipped by default**. Enable with `TGAPDNS_MULTICLIENT=1` (requires a generic ESP32-S3 dev module; AtomS3 reports slot 0 for every client so OR-combine cannot be verified there). See `test/conftest.py`.
- e2e tests connect to the board over WebSocket port **81** and read USB HID events via python-evdev.

## Architecture Notes
- Web UI lives in `webpage.h` (embedded HTML/JS/CSS), not a separate web server project.
- Firmware: `tgpadns.ino`. Entry point is standard Arduino setup()/loop().
- WebSocket protocol on port 81: key-down = token string (`*X`, `*DPAD:0`), key-up = `~<token>`, analog sticks scaled `-127..127 → -32768..32767`.

## Verified Status
- Offline tests: **23/23** passing
- Hardware e2e (AtomS3): **6 passed**, 0 failed — B button, face buttons, shoulder/menu buttons, left stick axis, D-pad hat, sticky watchdog survival
- Hardware e2e + multiclient (generic ESP32-S3): **10/10 passed** — all basic tests plus two-client independent, same-button no-stuck, L3 disconnect, four-step L3+R3
