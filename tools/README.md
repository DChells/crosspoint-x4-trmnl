# Tools

## flash_app1_usb.sh

Safely flashes the **application image only** into the secondary OTA slot (`ota_1`).

This is intended for development when you want to avoid overwriting CrossPoint's bootloader/partition table.

Usage:

```bash
pio run -e x4_dashboard

# Default port and bin path
tools/flash_app1_usb.sh

# Custom port
tools/flash_app1_usb.sh /dev/ttyACM1

# Custom port + custom bin path
tools/flash_app1_usb.sh /dev/ttyACM0 .pio/build/x4_dashboard_dev/firmware.bin
```

Notes:
- The offset `0x650000` matches CrossPoint's default `partitions.csv` for `ota_1`. Confirm for your build.
- On Linux, ModemManager can interfere with `/dev/ttyACM*`.
