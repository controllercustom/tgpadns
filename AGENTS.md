# AGENTS.md — TGPad-NS

## Build & Upload

Build profiles are defined in `sketch.yaml` with local library paths. The default profile is **atoms3**.

### Compile
```bash
# Default target: M5Stack AtomS3 (8MB flash) — verified target
arduino-cli compile .

# Generic ESP32-S3
arduino-cli compile --profile esp32s3 .

# LilyGo T-Dongle-S3 (160x80 ST7735 via vendored TFT_eSPI)
arduino-cli compile --profile tdongle_s3 .

# LilyGo T-Dongle-S3-Plus (same FQBN as base; define T_DONGLE_S3_PLUS for the banner)
arduino-cli compile --profile tdongle_s3_plus . --build-property compiler.cpp.extra_flags=-DT_DONGLE_S3_PLUS

# Waveshare ESP32-S3-Touch-LCD-1.54 (240x240 ST7789 via Arduino_GFX; core 3.2.0;
# the esp32 core 3.2.0 is downloaded into the profile cache on first build)
arduino-cli compile --profile waveshare . --build-property compiler.cpp.extra_flags=-DT_WAVESHARE_154
```

### Serial upload (board must be in download mode, BOOT button held)
```bash
# AtomS3
arduino-cli upload -p /dev/ttyACM0 --profile atoms3 .

# Generic ESP32-S3
arduino-cli upload -p /dev/ttyUSB0 --profile esp32s3 .

# T-Dongle-S3 / Plus (native USB CDC, typically /dev/ttyACM0;
# use the --build-property flag for the Plus variant)
arduino-cli upload -p /dev/ttyACM0 --profile tdongle_s3 .

# Waveshare ESP32-S3-Touch-LCD-1.54 (native USB CDC, /dev/ttyACM0;
# use the --build-property flag for -DT_WAVESHARE_154)
arduino-cli upload -p /dev/ttyACM0 --profile waveshare .
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
- **Version number must match in both files**: `#define VERSION "x.y.z"` in `tgpadns.ino` and the `<title>TGPad-NS x.y.z Gamepad</title>` in `webpage.h`. Bump **both** together when releasing.
- **OTA authentication**: Uncomment `#define OTA_PASS "your-password-here"` in `tgpadns.ino` to enable password-protected OTA (both ArduinoOTA and web-based). When enabled, append `--upload-field password=<pass>` to OTA upload commands and use basic auth user `admin` for web OTA.
- **Detecting successful OTA**: Do NOT parse espota.py or curl output — both produce `\r` progress bars that get truncated by tool output limits. Always check exit code (`$?`) instead: 0 = success, non-zero = failure. Append `; echo "EXIT=$?"` to verify.
- **`CDCOnBoot=default`** is mandatory for the generic ESP32-S3 (`USBMode=default`): the `cdc` value there activates the core's TinyUSB CDC (`Serial` = USBSerial), which conflicts with NSLiteController's own TinyUSB HID init. This does NOT apply to the T-Dongle/Waveshare boards — those run `USBMode=hwcdc` where `Serial` is the USB-Serial/JTAG peripheral, so `CDCOnBoot=cdc` is safe there (mirrors the verified tgpadxb setup).
- **Display (T-Dongle-S3 / Plus)**: Status on the 160×80 ST7735 via **vendored TFT_eSPI** (`lib/TFT_eSPI`, version 2.5.43). Auto-detected at compile time with `__has_include(<TFT_eSPI.h>)` **and** `ARDUINO_USB_MODE == 1` (i.e. `USBMode=hwcdc`, which all tdongle profiles select) → defines `T_DONGLE_S3`. The USB-mode guard prevents a stray global TFT_eSPI from enabling the T-Dongle path on AtomS3/generic builds. The vendored library's `User_Setup.h` is the T-Dongle-S3 config (ST7735 GREENTAB160x80, BGR, pins MOSI=3 SCLK=5 CS=4 DC=2 RST=1, backlight GPIO38 active-low 0=on). Do not add/remove that marker. The Plus is the same board plus `-DT_DONGLE_S3_PLUS` (only used for a boot banner; display/reset identical).
- **Display (Waveshare ESP32-S3-Touch-LCD-1.54)**: Status on the 1.54" 240×240 ST7789 via **Arduino_GFX** (GFX Library for Arduino **1.6.0**, core **3.2.0**). TFT_eSPI 2.5.43 crashed on this board (StoreProhibited in `begin_tft_write`) on both core 3.x and 2.0.x — the T-Dongle's ST7735 continues to work on TFT_eSPI 2.5.43, so the crash is Waveshare-specific. Selected EXPLICITLY by the `-DT_WAVESHARE_154` build flag (defined before any `__has_include` probe) → defines `WAVESHARE_154` and includes `<Arduino_GFX_Library.h>`. The flag must be checked BEFORE the T-Dongle probe because both boards use `USBMode=hwcdc` — otherwise a waveshare build could mis-detect as `T_DONGLE_S3`. Display is wrapped by the `WS154Display` adapter in `tgpadns.ino`, which exposes the TFT_eSPI API subset the sketch uses on top of Arduino_GFX (`new Arduino_ST7789(new Arduino_ESP32SPI(45, 21, 38, 39, -1), 40, 0, true, 240, 240)`; `printf()` is buffered via `vsnprintf`). Display config: ST7789 240×240, pins MOSI=39 SCLK=38 CS=21 DC=45 RST=40, backlight GPIO46 (active-high, set HIGH in setup). Do not add/remove that marker. The adapter calls `setTextWrap(false)` in `init()` — Arduino_GFX defaults `wrap=true`, which makes `getTextBounds` under-measure strings wider than the panel, breaking `printWrap`'s overflow detection and truncating long lines like the title. With wrap off, `textWidth()` reports true width and `printWrap` handles line breaks.
- **USB mode + TinyUSB**: T-Dongle/Waveshare run `USBMode=hwcdc`; NSLiteController's `USB.begin()`/`tinyusb_init()` takes the USB port over from the built-in USB-Serial/JTAG — the USB serial console is lost at runtime on those boards. The sketch routes all `Serial` output to UART0 (`Serial0`) on non-AtomS3 boards (`#define Serial Serial0`) so debug output is always reachable on the UART0 pins.
- **Web serial flasher** (`docs/`): the board `<select>` must list all supported boards (`atoms3`, `esp32s3`, `tdongle_s3`, `tdongle_s3_plus`, `waveshare`); each has a matching `#<board>-info` bootloader-instructions block that `app.js` toggles. When adding a board, update: the `<option>`, the `#<board>-info` block, the `els` map in `app.js`, and `updateInstructions()`. Keep the list in sync with the release build loop that emits `firmware/<version>/<board>/` artifacts for `firmware.json`.
- **Libraries in `lib/` are git submodules** (`lib/ESP32nslite`, `lib/WiFiManager`, `lib/WebSockets`, `lib/M5GFX`). Profiles handle them via `dir:` entries — no manual `--library` flags needed when using profiles. **`lib/TFT_eSPI` is vendored** (not a submodule); it is the T-Dongle-configured copy, committed directly — it is also the compile-time marker that selects the T-Dongle display path.
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

- Multi-client e2e tests are **skipped by default** (so CI without hardware doesn't fail). Enable with `TGAPDNS_MULTICLIENT=1`. Verified to pass on **all five boards** (generic ESP32-S3 dev module, M5Stack AtomS3, LilyGo T-Dongle-S3, T-Dongle-S3-Plus, Waveshare ESP32-S3-Touch-LCD-1.54 — the AtomS3 report of "slot 0 for every client" was a misdiagnosis: slot index is pure WebSocketsServer library logic and display-independently board-agnostic). See `test/conftest.py`.
- e2e tests connect to the board over WebSocket port **81** and read USB HID events via python-evdev.

## Architecture Notes
- Web UI lives in `webpage.h` (embedded HTML/JS/CSS), not a separate web server project.
- Firmware: `tgpadns.ino`. Entry point is standard Arduino setup()/loop().
- WebSocket protocol on port 81: key-down = token string (`*X`, `*DPAD:0`), key-up = `~<token>`, analog sticks scaled `-127..127 → -32768..32767`.

## Verified Status
- Offline tests: **23/23** passing
- Hardware e2e (`TGAPDNS_MULTICLIENT=1`): **10/10 passed** on **all five boards** — M5Stack AtomS3, generic ESP32-S3 dev module, LilyGo T-Dongle-S3, LilyGo T-Dongle-S3-Plus, Waveshare ESP32-S3-Touch-LCD-1.54 (all basic tests plus two-client independent, same-button no-stuck, L3 disconnect, four-step L3+R3)
- Compile: **PASS** — `atoms3`, `esp32s3`, `tdongle_s3` / `tdongle_s3_plus` (core 3.3.10 + TFT_eSPI), `waveshare` (core 3.2.0 + GFX 1.6.0)
- Multi-client OR-combine is **board-agnostic**: the per-client slot index (`num`) is pure `WebSocketsServer` library logic (first-free slot at `newClient()`, fixed at `begin()`; `WEBSOCKETS_SERVER_CLIENT_MAX=5`) and is unaffected by the AtomS3 display (`#ifdef ARDUINO_M5STACK_ATOMS3`, SPI3/PWM7 only). The earlier "AtomS3 reports slot 0 for every client" claim was a misdiagnosis.
