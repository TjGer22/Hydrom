# Hydrom Firmware

This directory contains the ESP32 firmware for the Hydrom smart hydrometer, built with [PlatformIO](https://platformio.org/) on the Arduino framework.

## Target Hardware

**Heltec WiFi LoRa 32 V2** — ESP32 with integrated OLED and LoRa (only the ESP32 core is used; LoRa is not used in this project).

## Directory Structure

```
firmware/
├── src/              # main.cpp — application entry point
├── lib/              # Project-specific libraries
│   ├── ArduAutoUpdater/   # OTA firmware update
│   ├── BTManager/         # Bluetooth LE (BLE iBeacon)
│   ├── Calibrator/        # Gravity calibration logic
│   ├── DeviceManager/     # Device lifecycle management
│   ├── FileManager/       # SPIFFS configuration storage
│   ├── FindCurve/         # Polynomial curve fitting
│   ├── I2Cdevlib-Core/    # I2C abstraction (vendored)
│   ├── I2Cdevlib-MPU6050/ # MPU6050 IMU driver (vendored)
│   ├── NetworkManager/    # Wi-Fi (AP + client mode)
│   ├── SensorManager/     # IMU + DS18B20 sensor handling
│   ├── ServiceManager/    # Cloud service integrations
│   ├── Utils/             # Shared utilities
│   └── WebManager/        # Embedded Web UI
├── data/             # SPIFFS assets (logo, font, favicon)
├── Flasher/          # Installer scripts and helper scripts
├── platformio.ini    # Build configuration
└── partitions_custom8MB.csv  # Custom 8 MB partition table
```

---

## Installing the Firmware (no development tools required)

The installer scripts download the latest release from GitHub and flash it
to your Hydrom automatically. You only need an internet connection and a USB
cable that supports data transfer.

### Windows

Open **PowerShell** (search for it in the Start menu) and paste:

```powershell
powershell -ExecutionPolicy Bypass -Command "irm https://raw.githubusercontent.com/TjGer22/Hydrom/main/firmware/Flasher/install.ps1 | iex"
```

The script will:

1. Install Python 3 if it is not already present (via winget or direct download)
2. Install esptool via pip
3. Detect the COM port of your Hydrom automatically
4. Download the latest firmware files from the GitHub Releases page
5. Flash all five binary files in the correct order

> **Driver note** — if no COM port is detected, install the CP2102 driver first:
> https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers

---

### macOS

Open **Terminal** and paste:

```bash
curl -fsSL https://raw.githubusercontent.com/TjGer22/Hydrom/main/firmware/Flasher/install.sh | bash
```

---

### Linux

```bash
curl -fsSL https://raw.githubusercontent.com/TjGer22/Hydrom/main/firmware/Flasher/install.sh | bash
```

> **Permission note** — if the flash fails with a permission error, add your user
> to the `dialout` group, then log out and back in:
> ```bash
> sudo usermod -aG dialout $USER
> ```

---

### Overriding port or baud rate

If the installer cannot find the device automatically, you can pass the port
and baud rate as environment variables before the command:

**macOS / Linux:**
```bash
HYDROM_PORT=/dev/ttyUSB0 HYDROM_BAUD=115200 \
  curl -fsSL https://raw.githubusercontent.com/TjGer22/Hydrom/main/firmware/Flasher/install.sh | bash
```

**Windows (PowerShell):**
```powershell
$env:HYDROM_PORT="COM3"; $env:HYDROM_BAUD="115200"
powershell -ExecutionPolicy Bypass -Command "irm https://raw.githubusercontent.com/TjGer22/Hydrom/main/firmware/Flasher/install.ps1 | iex"
```

---

### What happens during flashing

The installer writes five binary files to specific addresses in the ESP32 flash:

| File | Address | Purpose |
|------|---------|---------|
| `bootloader_dio_40m.bin` | `0x1000` | ESP32 first-stage bootloader |
| `partitions.bin` | `0x8000` | Partition table (8 MB layout) |
| `boot_app0.bin` | `0xe000` | OTA selection bootloader |
| `firmware.bin` | `0x10000` | Application firmware |
| `spiffs.bin` | `0x5B0000` | Filesystem (Web UI assets) |

When prompted, hold the **BOOT** button on the Hydrom and release it once
you see `Connecting...` in the output.

---

## Building from Source (PlatformIO)

```bash
# Build firmware + SPIFFS image
pio run --environment local --target buildfs
pio run --environment local

# Flash to connected device
pio run --environment local --target upload
pio run --environment local --target uploadfs

# Open serial monitor
pio device monitor
```

## Credentials

Copy `lib/FileManager/Credentials.txt.example` to `lib/FileManager/Credentials.txt` and fill in your own values.
The `Credentials.txt` file is listed in `.gitignore` and will **never** be committed.

## License

GPL v3 — see [../LICENSE](../LICENSE)
