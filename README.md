# TRMNL Dashboard for Xteink X4

A TRMNL-compatible dashboard firmware for the Xteink X4 e-paper display, integrating with the CrossPoint ecosystem.

## Features

- **TRMNL-Compatible**: Works with official TRMNL servers or self-hosted BYOS
- **Configurable Backend**: Supports both TRMNL official API and self-hosted servers
- **SD Card Configuration**: No WiFi captive portal - simple JSON config file
- **Error Display**: Visual error screens for all failure modes (no silent failures)
- **Deep Sleep Power Management**: Efficient power usage with timer and button wake
- **CrossPoint Integration**: Can be installed as a CrossPoint app or standalone firmware

## Hardware Requirements

- **Xteink X4** e-paper display (ESP32-C3 based)
- **SD Card** (for configuration file)
- **WiFi Network** (2.4GHz)

## Installation

### Option 1: CrossPoint App (Recommended)

1. Download `app.bin` from the [Releases](../../releases) page
2. Copy it into the CrossPoint apps directory on the SD card:
   - `/.crosspoint/apps/trmnl-dashboard/app.bin`
3. Create `/trmnl-config.json` on the SD card (see Configuration below)
4. Create the app manifest:
   - `/.crosspoint/apps/trmnl-dashboard/app.json` (see `app.json` in this repo)
5. On the device: Home → Apps → TRMNL Dashboard → Install

Notes:
- `app.bin` is the **application image only** (PlatformIO/ESP-IDF app binary). It should be flashed into the app OTA slot (typically `ota_1`).
- Do NOT flash bootloader/partition table when installing as an app.

Fast iteration option:
- Recommended: upload the release `app.zip` (contains `app.bin` + `app.json`) via File Transfer → Apps.
- Alternative: upload `app.bin` only (CrossPoint will generate `app.json`, but you'll need to provide App ID in the UI).

### Option 2: Standalone Flash

WARNING: This replaces the device firmware and can overwrite CrossPoint (bootloader/partition table/app slots).
Only use this if you intend to run the TRMNL firmware as the primary firmware.

1. Clone this repository:
   ```bash
   git clone https://github.com/yourusername/trmnl-x4-dashboard.git
   cd trmnl-x4-dashboard
   ```

2. Install dependencies:
   ```bash
   pip install platformio
   ```

3. Build and flash (will overwrite firmware on the device):
   ```bash
   pio run -e x4_dashboard -t upload
   ```

### Safe Development Flashing (App Slot Only)

If you want to avoid overwriting CrossPoint and only write the **secondary OTA slot** (app partition), you can flash the app image directly.

CrossPoint's partition table typically uses:
- `ota_0` at `0x10000`
- `ota_1` at `0x650000`

To flash this firmware into `ota_1` only:

```bash
# Build the application image
pio run -e x4_dashboard

# The app image is:
#   .pio/build/x4_dashboard/firmware.bin

# Flash ONLY the ota_1 slot (does NOT touch bootloader/partitions)
esptool.py --chip esp32c3 --port /dev/ttyACM0 --baud 460800 write_flash 0x650000 .pio/build/x4_dashboard/firmware.bin
```

If your CrossPoint build uses a different partition table, confirm offsets in its `partitions.csv` first.

If you're using the CrossPoint AppLoader PR, prefer the on-device install flow instead of manual `esptool.py`.

## Configuration

Copy `trmnl-config.example.json` to your SD card root as `/trmnl-config.json` and edit:

```json
{
  "wifi_ssid": "YOUR_WIFI_SSID",
  "wifi_password": "YOUR_WIFI_PASSWORD",
  "server_url": "https://usetrmnl.com",
  "api_key": "YOUR_API_KEY",
  "device_id": "",
  "refresh_interval": 1800,
  "use_insecure_tls": true,
  "standalone_mode": false
}
```

### Configuration Fields

- **wifi_ssid** (required): Your WiFi network name
- **wifi_password** (required): Your WiFi password
- **server_url** (required): TRMNL server URL (`https://usetrmnl.com` for official)
- **api_key** (required): Your TRMNL API key
- **device_id** (optional): Custom device ID. Leave empty to use WiFi MAC address
- **refresh_interval** (optional): Seconds between updates (default: 1800 = 30 minutes)
- **use_insecure_tls** (optional): Skip TLS certificate validation (default: true for MVP)
- **standalone_mode** (optional): If true, Back button is ignored (default: false)

## Getting an API Key

### Official TRMNL Server

1. Create an account at [usetrmnl.com](https://usetrmnl.com)
2. Register your device in the dashboard
3. Copy the API key to your config file

### Self-Hosted (BYOS)

1. Set up a BYOS server ([byos_laravel](https://github.com/usetrmnl/byos_laravel) recommended):
   ```bash
   git clone https://github.com/usetrmnl/byos_laravel
   cd byos_laravel
   docker compose -f docker-compose.yml up -d
   ```

2. Access dashboard at `http://localhost:4567`
3. Add your device and get API key
4. Set `server_url` to your server IP/port

## Usage

Once configured and powered on:

1. Device boots and reads config from SD card
2. Connects to WiFi
3. Fetches image from configured server
4. Displays image on e-paper screen
5. Enters deep sleep until next refresh interval

### Manual Controls

- **Power Button**: Wake device for immediate refresh
- **Back Button**: Exit to CrossPoint launcher (if installed via CrossPoint)

## Troubleshooting

### "Insert SD Card" Error
- Ensure SD card is formatted FAT32
- Check card is properly inserted
- Verify config file exists at `/trmnl-config.json`

### "Config file missing" Error
- Check config file is named exactly `trmnl-config.json` (not `.txt` or other extension)
- Verify file is in SD card root (not in a folder)
- Ensure JSON syntax is valid (check for trailing commas)

### "WiFi failed" Error
- Verify SSID and password are correct
- Ensure 2.4GHz network (5GHz not supported on ESP32-C3)
- Check WiFi signal strength

### "API Error" Error
- Verify API key is correct
- Check server URL is accessible
- For self-hosted: ensure server is running and accessible

### Device Won't Wake
- Power button must be held briefly (not just pressed)
- Check battery level if using battery power
- Verify deep sleep GPIO wake is working

### Device Looks "Bricked" After Flashing

This is usually a soft-brick caused by a crash loop or deep sleep happening immediately after boot.

Mitigations:
- Use the development environment to disable deep sleep:
  ```bash
  pio run -e x4_dashboard_dev
  ```
- Linux: ModemManager can grab `/dev/ttyACM0` and make flashing/monitoring flaky:
  ```bash
  sudo systemctl stop ModemManager
  ```
  (For a permanent fix, add a udev rule to ignore Espressif USB CDC: `idVendor=303a`, `idProduct=1001`.)

## Building from Source

```bash
# Install PlatformIO
pip install platformio

# Clone repository
git clone https://github.com/yourusername/trmnl-x4-dashboard.git
cd trmnl-x4-dashboard

# Build
pio run -e x4_dashboard

# Flash to device
pio run -e x4_dashboard -t upload

# Monitor serial output
pio device monitor
```

## Technical Details

- **Platform**: ESP32-C3 (RISC-V)
- **Display**: 800×480 1-bit e-paper (SSD1677 controller)
- **Image Format**: 1-bit monochrome BMP (800×480)
- **Runtime Model**: Single-shot (boot → fetch → render → deep sleep)
- **SDK**: open-x4-sdk (community SDK for X4)

## License

MIT License - See LICENSE file for details

## Contributing

Contributions welcome! Please submit pull requests or open issues for bugs and feature requests.

## Acknowledgments

- TRMNL team for the excellent dashboard platform
- open-x4-sdk contributors
- CrossPoint community
