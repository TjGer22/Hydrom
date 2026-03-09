---
description: >
  Overview of the Hydrom hardware design, KiCad source files and PCB specifications.
---

# Hardware Overview

The Hydrom PCB is designed with [KiCad](https://www.kicad.org/) (version 6+). All design files are open source under the **CERN Open Hardware Licence v2 – Strongly Reciprocal (CERN-OHL-S v2)**.

## Board Images

| Top side | Bottom side |
|---|---|
| ![PCB Top](../assets/images/Folie1.png) | ![PCB Bottom](../assets/images/Folie1.png) |

## Main Components

| Component | Function |
|---|---|
| ESP32 (Heltec WiFi LoRa 32 V2) | Main microcontroller — Wi-Fi, BLE, processing |
| MPU6050 | 6-axis IMU — measures tilt angle for density calculation |
| DS18B20 | 1-Wire temperature sensor |
| RT9080-33GJ5 | 3.3 V LDO voltage regulator |
| LiPo battery | Power supply |
| USB-C connector | Charging |

## Design Files

All KiCad source files are in [`hardware/pcb/`](https://github.com/TjGer22/Hydrom/tree/main/hardware/pcb). Open `hydrom_V1.0-2.kicad_pro` in KiCad to get started.

## Production Files

Ready-to-order files for PCB fabrication and assembly are in [`hardware/production/`](https://github.com/TjGer22/Hydrom/tree/main/hardware/production):

- **Gerbers** — standard Gerber 274-X format, tested with JLCPCB
- **BOM** — full bill of materials in XLSX format
- **Centroid** — pick-and-place files for top and bottom side
