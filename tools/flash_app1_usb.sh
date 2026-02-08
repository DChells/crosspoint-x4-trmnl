#!/usr/bin/env bash
set -euo pipefail

# Flash ONLY the secondary OTA slot (ota_1) with the app image.
# This does NOT touch bootloader/partition table and is the safest way
# to iterate when CrossPoint lives in the other slot.

PORT="${1:-/dev/ttyACM0}"
BIN_PATH="${2:-.pio/build/x4_dashboard/firmware.bin}"

# CrossPoint default partitions.csv typically has ota_1 at 0x650000.
OFFSET="0x650000"

if [[ ! -f "${BIN_PATH}" ]]; then
  echo "ERROR: App image not found: ${BIN_PATH}" >&2
  echo "Build first: pio run -e x4_dashboard" >&2
  exit 1
fi

echo "Flashing app image to ota_1 (${OFFSET})"
echo "Port: ${PORT}"
echo "Bin:  ${BIN_PATH}"

python3 -m esptool --chip esp32c3 --port "${PORT}" --baud 460800 write_flash "${OFFSET}" "${BIN_PATH}"

echo "Done. If the device boots into a loop, switch back to CrossPoint slot or reflash CrossPoint." 
