#!/bin/bash
PROJECT="tgpadns"
ARDDIR=/tmp/acli_${PROJECT}_$$

export ARDUINO_BOARD_MANAGER_ADDITIONAL_URLS="https://espressif.github.io/arduino-esp32/package_esp32_index.json"
export ARDUINO_LIBRARY_ENABLE_UNSAFE_INSTALL=1
export ARDUINO_DIRECTORIES_DATA="${ARDDIR}/data"
export ARDUINO_DIRECTORIES_USER="${ARDDIR}/user"
export LIBDIR="${ARDUINO_DIRECTORIES_USER}/libraries"

mkdir -p ${LIBDIR}

arduino-cli core --no-color update-index
arduino-cli core --no-color install "esp32:esp32@3.3.10"

# Libraries are git submodules in lib/ — explicit paths required (auto-discovery broken for shallow submodules)

arduino-cli lib --no-color list

LIBFLAGS=(--library "lib/ESP32nslite" --library "lib/WiFiManager" --library "lib/WebSockets" --library "lib/M5GFX")

# Build both targets using profiles from sketch.yaml
for profile in atoms3 esp32s3; do
    echo "=== Building profile: ${profile} ==="
    arduino-cli compile --clean \
        "${LIBFLAGS[@]}" \
        --output-dir "./build/${profile}" . || exit 1
done

echo ""
echo "Build complete. Binaries in build/atoms3/ and build/esp32s3/"

# Run tests (offline, no hardware)
python3 -m pytest test/ -v --ignore=test/e2e
