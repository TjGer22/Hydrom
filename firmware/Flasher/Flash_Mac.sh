#!/usr/bin/env bash
# =============================================================================
#  Flash_Mac.sh — Hydrom Firmware Flash Script (macOS helper)
# =============================================================================
#  Author : TjGer22
#  Repo   : https://github.com/TjGer22/Hydrom
#  SPDX-License-Identifier: GPL-3.0-or-later
#
#  Convenience wrapper for macOS that resolves the PlatformIO esptool.py path
#  automatically and invokes Flash.sh with the correct arguments.
#
#  Usage:
#    ./Flash_Mac.sh <serial-port>
#    Example: ./Flash_Mac.sh /dev/cu.usbserial-210
#
#  Requirements:
#    - PlatformIO CLI installed (pio)
#    - Python 3 available on PATH
#    - Firmware already compiled, or run from the Flasher directory so
#      Flash.sh can trigger the PlatformIO build step.
# =============================================================================

set -euo pipefail

PORT="${1:-}"

if [[ -z "$PORT" ]]; then
    echo "Usage: $0 <serial-port>"
    echo "  Example: $0 /dev/cu.usbserial-210"
    exit 1
fi

PYTHON=$(which python3)
ESPTOOL=$(find "${HOME}/.platformio/packages/tool-esptoolpy" -name "esptool.py" 2>/dev/null | head -1)
PIO_BUILD="../.pio/build/local"
FLASH_DIR="$(dirname "$0")"

if [[ -z "$ESPTOOL" ]]; then
    echo "ERROR: esptool.py not found. Make sure PlatformIO is installed."
    exit 1
fi

sh "${FLASH_DIR}/Flash.sh" \
    "$PYTHON" \
    "$ESPTOOL" \
    "$PORT" \
    "${HOME}/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32/bin/bootloader_dio_40m.bin" \
    "${PIO_BUILD}/partitions.bin" \
    "${HOME}/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin" \
    "${PIO_BUILD}/firmware.bin" \
    "${PIO_BUILD}/spiffs.bin"
