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
├── Flasher/          # Helper scripts for manual flashing
├── platformio.ini    # Build configuration
└── partitions_custom8MB.csv  # Custom 8 MB partition table
```

## Building

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
