# Hydrom Firmware – Code Analysis & Cleanup Plan

> Generated: 2026-03-08
> Branch: `cleanup`
> Status: **In progress**

This document tracks every finding from the deep code review so work can be
resumed if the session is interrupted. Each item is marked ✅ when fixed.

---

## 1. Critical Typos in Identifiers

These affect compilation and must be changed everywhere consistently.

| # | File | Identifier | Issue | Fix | Status |
|---|------|-----------|-------|-----|--------|
| T1 | `lib/SensorManager/TemperaturCompensation.h/.cpp` | `TemperaturCompensation` | Missing 'e' → affects 17+ files | Rename class + files to `TemperatureCompensation` | ✅ |
| T2 | `lib/FileManager/HydromConfiguration.h:148` | `Enable_Temperaturcompensation` | Typo in member name | `Enable_TemperatureCompensation` | ✅ |
| T3 | `lib/FileManager/HydromConfiguration.h` | `Temperaturcompensation_Factor` | Typo in member | `TemperatureCompensation_Factor` | ✅ |
| T4 | `lib/SensorManager/SensorManager.h:47` | `readBatteryVolatage` | "Volatage" → "Voltage" | `readBatteryVoltage` | ✅ |
| T5 | `src/main.cpp:95` | comment `"3 seconds0"` | Stray zero | `"3 seconds"` | ✅ |

### Files affected by T1 (`TemperaturCompensation`):
- `lib/SensorManager/TemperaturCompensation.h` → rename file
- `lib/SensorManager/TemperaturCompensation.cpp` → rename file
- `lib/SensorManager/SensorManager.h` (include + member type)
- `lib/SensorManager/SensorManager.cpp` (usage)
- `lib/FileManager/HydromConfiguration.h` (member)
- `lib/FileManager/HydromConfiguration.cpp` (usage)
- `lib/WebManager/WebManager.cpp` (usage)
- `src/main.cpp` (include + usage)

---

## 2. Magic Numbers → Named Constants

All to be placed in `lib/FileManager/Globals.h`.

| # | File | Line | Value | Name | Status |
|---|------|------|-------|------|--------|
| M1 | `src/main.cpp` | 96 | `7` (button press seconds) | `BUTTON_PRESS_RESET_WIFI_SECONDS` | ✅ |
| M2 | `src/main.cpp` | 101 | `15` (factory reset seconds) | `BUTTON_PRESS_FACTORY_RESET_SECONDS` | ✅ |
| M3 | `src/main.cpp` | 170 | `3.0` (battery critical V) | `BATTERY_CRITICAL_VOLTAGE` | ✅ |
| M4 | `src/main.cpp` | 170 | `1.0` (battery min V) | `BATTERY_MIN_VOLTAGE` | ✅ |
| M5 | `src/main.cpp` | 173 | `10.0` (min startup temp °C) | `MIN_STARTUP_TEMPERATURE` | ✅ |
| M6 | `src/main.cpp` | 175 | `30.0` (max startup temp °C) | `MAX_STARTUP_TEMPERATURE` | ✅ |
| M7 | `lib/Calibration_MPU.cpp` | 36 | `3150` (µs delay) | `MPU_CALIBRATION_US_DELAY` | ✅ |
| M8 | `lib/Calibration_MPU.cpp` | 37 | `1000` (fast samples) | `MPU_CALIBRATION_FAST_SAMPLES` | ✅ |
| M9 | `lib/Calibration_MPU.cpp` | 38 | `10000` (slow samples) | `MPU_CALIBRATION_SLOW_SAMPLES` | ✅ |
| M10 | `lib/BTManager/BLESender.cpp` | 76 | `0x4C00` | `APPLE_IBEACON_MANUFACTURER_ID` | ✅ |

---

## 3. Duplicate Logic → Helper Functions

| # | Location | Pattern | Refactoring | Status |
|---|----------|---------|------------|--------|
| D1 | `main.cpp:209,281,334,446` | `DeepSleep.hours*3600 + DeepSleep.minutes*60 + DeepSleep.seconds` | Extract `calculateDeepSleepSeconds()` to Util | ✅ |
| D2 | `SensorManager.cpp:49-58` | Element-by-element array copy | Replace with `memcpy()` | ✅ |

---

## 4. const Correctness

| # | File | Function | Fix | Status |
|---|------|---------|-----|--------|
| C1 | `SensorManager.h` | `get_batteryVoltage()` and all getters | Add `const` | ✅ |
| C2 | `FindCurve.h:29` | `get_Curve_Quality()` | Add `const` | ✅ |
| C3 | `ServiceManager.h:33` | `Replace_Placeholder(char _url[265])` | `const String&` param | ✅ |
| C4 | `Calibration_MPU.h:22-26` | `LBRACKET`, `RBRACKET` etc. | `static constexpr` | ✅ |

---

## 5. Safety – StringCopy Buffer Overflow

| # | File | Lines | Issue | Fix | Status |
|---|------|-------|-------|-----|--------|
| S1 | `Util.cpp:13-21` | `StringCopy` + `StringCopyConst` | `strlen(des)` on uninit buffer; no bounds check | Add null-guard + use `strlcpy()` | ✅ |
| S2 | `Util.cpp:27` | `IpToString()` static buffer | Not thread-safe | Add comment, keep for now (embedded single-task path) | ✅ |

---

## 6. Include Guards → `#pragma once`

All custom headers converted to `#pragma once` (PlatformIO/GCC supports it).

Files: `TemperaturCompensation.h`, `SensorManager.h`, `FindCurve.h`,
`Calibrator.h`, `BLESender.h`, `NetworkManager.h`, `DeviceManager.h`,
`WebManager.h`, `ServiceManager.h`, `HydromConfiguration.h`.

Status: ✅

---

## 7. Uninitialized Variables

| # | File | Line | Variable | Fix | Status |
|---|------|------|---------|-----|--------|
| U1 | `main.cpp:34` | `long last_Release;` | Initialize to `0` | ✅ |
| U2 | `SensorManager.h:88-100` | `float g_*` members | Default-initialize to `0.0f` | ✅ |

---

## 8. Dead Code Removed

| # | File | Lines | Description | Status |
|---|------|-------|-------------|--------|
| DC1 | `main.cpp:554-605` | ~50-line commented-out block | Remove | ✅ |
| DC2 | `WebManager.cpp:30` | `// boolean liveData = false;` | Remove comment | ✅ |

---

## 9. Out of Scope / Risk Too High (deferred)

These require deeper refactoring or embedded system expertise:

- **Race conditions** on global structs (FreeRTOS multi-core): Requires adding `SemaphoreHandle_t`; risks affecting timing.
- **setup() split**: 280-line function; refactoring into sub-functions is safe but very large change.
- **Hardcoded OTA password** in `NetworkManager.cpp:40`: Already in `Secrets.h`-like pattern; moving to runtime config requires flash storage changes.
- **TLS `setInsecure()`** in `ArduAutoUpdater`: Requires embedding CA cert bundle; separate task.
- **`Salzen()` rename**: Cryptographic/obfuscation function – renaming without understanding all callers is risky.
- **Service registry pattern** for `publish_Data_to_Services()`: Large architectural change.

---

## File Rename Map (T1)

| Old | New |
|-----|-----|
| `lib/SensorManager/TemperaturCompensation.h` | `lib/SensorManager/TemperatureCompensation.h` |
| `lib/SensorManager/TemperaturCompensation.cpp` | `lib/SensorManager/TemperatureCompensation.cpp` |

---

*Resume point: All items with ✅ are complete. Continue with any remaining items.*
