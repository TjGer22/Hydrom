#!/usr/bin/env bash
# =============================================================================
#  Flash.sh — Hydrom Firmware Build & Flash Script
# =============================================================================
#  Author : TjGer22
#  Repo   : https://github.com/TjGer22/Hydrom
#  SPDX-License-Identifier: GPL-3.0-or-later
#
#  Builds the firmware and SPIFFS image with PlatformIO, then flashes the
#  compiled binaries to a connected ESP32 device using esptool.py.
#
#  Usage (called by install.sh with positional arguments):
#    $1  python executable
#    $2  esptool.py path
#    $3  serial port (e.g. /dev/ttyUSB0)
#    $4  bootloader binary
#    $5  partition table binary
#    $6  boot_app0 binary
#    $7  firmware binary
#    $8  SPIFFS image
# =============================================================================

cd .. && platformio run --target buildfs --environment local && platformio run -e local && cd Flasher && $1 $2 --chip esp32 --connect-attempts 0 --port $3 --baud 460800 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0x1000 $4 0x8000 $5 0xe000 $6 0x10000 $7 5963776 $8
