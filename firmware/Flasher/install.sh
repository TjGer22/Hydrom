#!/usr/bin/env bash
# =============================================================================
#  Hydrom Firmware Installer — macOS & Linux
# =============================================================================
#  One-line install:
#    curl -fsSL https://raw.githubusercontent.com/TjGer22/Hydrom/main/firmware/Flasher/install.sh | bash
#
#  Optional environment overrides:
#    HYDROM_PORT=/dev/ttyUSB0   bash install.sh   # skip port detection
#    HYDROM_BAUD=115200         bash install.sh   # lower baud if connection fails
# =============================================================================

set -euo pipefail

# ── Constants ─────────────────────────────────────────────────────────────────
readonly REPO="TjGer22/Hydrom"
readonly CHIP="esp32"
readonly FLASH_MODE="dio"
readonly FLASH_FREQ="40m"

FLASH_BAUD="${HYDROM_BAUD:-460800}"
FORCED_PORT="${HYDROM_PORT:-}"
NO_INTERACTIVE="${HYDROM_NOINTERACTIVE:-}"   # set to any value to skip all prompts

# Testing seams — override via environment (used by CI; leave unset in normal use)
RELEASES_API="${HYDROM_API_URL:-https://api.github.com/repos/${REPO}/releases/latest}"
DOWNLOAD_BASE="${HYDROM_DOWNLOAD_BASE:-https://github.com/${REPO}/releases/download}"

# Flash map — order matters for esptool
readonly FILES=(
    "bootloader_dio_40m.bin"
    "partitions.bin"
    "boot_app0.bin"
    "firmware.bin"
    "spiffs.bin"
)
readonly ADDRS=(
    "0x1000"
    "0x8000"
    "0xe000"
    "0x10000"
    "0x5B0000"
)

# ── Colour output (disabled when not a TTY) ───────────────────────────────────
if [ -t 1 ]; then
    RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
    CYAN='\033[0;36m'; BOLD='\033[1m'; DIM='\033[2m'; NC='\033[0m'
else
    RED=''; GREEN=''; YELLOW=''; CYAN=''; BOLD=''; DIM=''; NC=''
fi

# ── Logging helpers ───────────────────────────────────────────────────────────
info()    { printf "${CYAN}  ▸  %s${NC}\n" "$*"; }
success() { printf "${GREEN}  ✔  %s${NC}\n" "$*"; }
warn()    { printf "${YELLOW}  ⚠  %s${NC}\n" "$*"; }
error()   { printf "${RED}  ✘  %s${NC}\n" "$*" >&2; }
header()  { printf "\n${BOLD}${CYAN}── %s ──────────────────────────────${NC}\n\n" "$*"; }
die()     { error "$*"; exit 1; }

# ── Read from /dev/tty (survives curl | bash) ─────────────────────────────────
ask() {
    # $1 = prompt, $2 = optional default
    local prompt="$1"
    local default="${2:-}"
    local reply
    if [ -n "$NO_INTERACTIVE" ]; then printf '%s' "$default"; return; fi
    printf "${YELLOW}  ?  %s${NC}" "$prompt"
    if [ -t 0 ]; then
        read -r reply
    else
        read -r reply </dev/tty
    fi
    printf '%s' "${reply:-$default}"
}

press_enter() {
    if [ -n "$NO_INTERACTIVE" ]; then return; fi
    printf "${YELLOW}  ➤  %s${NC}" "$1"
    if [ -t 0 ]; then
        read -r _
    else
        read -r _ </dev/tty
    fi
}

# ── Cleanup on exit ───────────────────────────────────────────────────────────
WORK_DIR=""
cleanup() { [ -n "$WORK_DIR" ] && rm -rf "$WORK_DIR"; }
trap cleanup EXIT

# ── OS detection ──────────────────────────────────────────────────────────────
OS=""
detect_os() {
    case "$(uname -s)" in
        Darwin) OS="macos" ;;
        Linux)  OS="linux" ;;
        *)      die "Unsupported OS: $(uname -s). This script supports macOS and Linux." ;;
    esac
    info "Operating system: $OS"
}

# ── Dependency: Python 3 ──────────────────────────────────────────────────────
PYTHON=""

find_python() {
    local ver
    for cmd in python3 python3.13 python3.12 python3.11 python3.10 python3.9 python; do
        if command -v "$cmd" &>/dev/null; then
            ver=$("$cmd" -c "import sys; print(sys.version_info.major)" 2>/dev/null) || continue
            if [ "${ver:-0}" -ge 3 ]; then
                PYTHON="$cmd"
                return 0
            fi
        fi
    done
    return 1
}

install_python() {
    warn "Python 3 not found — attempting to install..."
    if [ "$OS" = "macos" ]; then
        if command -v brew &>/dev/null; then
            info "Installing via Homebrew..."
            brew install python3
        else
            die "Python 3 is required. Install it from https://www.python.org/downloads/ then re-run this script."
        fi
    else
        # Linux — try common package managers
        if command -v apt-get &>/dev/null; then
            info "Installing via apt..."
            sudo apt-get update -qq
            sudo apt-get install -y python3 python3-pip
        elif command -v dnf &>/dev/null; then
            info "Installing via dnf..."
            sudo dnf install -y python3 python3-pip
        elif command -v pacman &>/dev/null; then
            info "Installing via pacman..."
            sudo pacman -Sy --noconfirm python python-pip
        elif command -v zypper &>/dev/null; then
            info "Installing via zypper..."
            sudo zypper install -y python3 python3-pip
        else
            die "Python 3 is required but could not be installed automatically.\nInstall it manually (https://www.python.org/downloads/) then re-run."
        fi
    fi
}

ensure_python() {
    if ! find_python; then
        install_python
        find_python || die "Python 3 installation failed. Please install manually."
    fi
    success "Python: $($PYTHON --version)"
}

# ── Dependency: esptool ───────────────────────────────────────────────────────
ESPTOOL_CMD=()   # array so spaces in "python3 -m esptool" are safe

find_esptool() {
    for cmd in esptool esptool.py; do
        if command -v "$cmd" &>/dev/null; then
            ESPTOOL_CMD=("$cmd")
            return 0
        fi
    done
    if "$PYTHON" -m esptool version &>/dev/null 2>&1; then
        ESPTOOL_CMD=("$PYTHON" "-m" "esptool")
        return 0
    fi
    return 1
}

ensure_esptool() {
    if find_esptool; then
        success "esptool: $("${ESPTOOL_CMD[@]}" version 2>&1 | head -1)"
        return
    fi

    # On modern macOS / Debian-based Linux the system Python is "externally
    # managed" (PEP 668) and rejects bare `pip install`.  Install esptool
    # into a dedicated venv so we never touch the system packages.
    local venv_dir="${HOME}/.hydrom_esptool_venv"

    # Remove any previous venv so we always get the latest esptool
    if [ -d "$venv_dir" ]; then
        info "Removing old esptool venv (${venv_dir})..."
        rm -rf "$venv_dir"
    fi
    info "Installing esptool into venv (${venv_dir})..."

    "$PYTHON" -m venv "$venv_dir" \
        || die "Could not create a Python venv. Make sure 'python3-venv' is installed."

    "${venv_dir}/bin/python" -m pip install --quiet --disable-pip-version-check --upgrade pip \
        || true  # non-critical — proceed even if pip self-upgrade fails
    "${venv_dir}/bin/python" -m pip install --quiet --disable-pip-version-check esptool \
        || die "esptool installation failed inside venv."

    # Add the venv's bin dir to PATH so find_esptool / subsequent calls work
    export PATH="${venv_dir}/bin:${PATH}"
    PYTHON="${venv_dir}/bin/python"

    if find_esptool; then
        success "esptool installed: $("${ESPTOOL_CMD[@]}" version 2>&1 | head -1)"
    else
        die "esptool installed into venv but still not found. Please report this issue."
    fi
}

# ── Serial port detection ─────────────────────────────────────────────────────
PORT=""

list_serial_ports() {
    # Testing seam: HYDROM_NO_PORTS forces an empty port list (used in CI to
    # verify non-interactive behaviour when no device is connected).
    [ -n "${HYDROM_NO_PORTS:-}" ] && return

    local ports=()
    if [ "$OS" = "macos" ]; then
        # CP2102 shows as cu.usbserial-*, CH340 as cu.wchusbserial-*, FTDI as cu.usbmodem-*
        while IFS= read -r p; do ports+=("$p"); done \
            < <(ls /dev/cu.usbserial-* /dev/cu.wchusbserial-* \
                   /dev/cu.SLAB_USBtoUART /dev/cu.usbmodem* 2>/dev/null | sort -u || true)
    else
        # Linux — check dialout group permission first
        if ! groups 2>/dev/null | grep -qE '\b(dialout|uucp|plugdev)\b'; then
            warn "Your user may not have permission to access serial ports."
            warn "If flashing fails, run:  sudo usermod -aG dialout \$USER"
            warn "Then log out, log back in, and retry."
        fi
        # ESP32/CP2102 → ttyUSB*, CDC-ACM → ttyACM*
        # /dev/ttyS* are built-in UARTs, never USB serial adapters — skip entirely
        for p in /dev/ttyUSB* /dev/ttyACM*; do
            [[ -e "$p" ]] || continue   # skip unmatched globs
            ports+=("$p")
        done
    fi
    printf '%s\n' "${ports[@]:-}"
}

detect_port() {
    if [ -n "$FORCED_PORT" ]; then
        PORT="$FORCED_PORT"
        info "Using port from environment: $PORT"
        return
    fi

    local ports=()
    while IFS= read -r p; do [ -n "$p" ] && ports+=("$p"); done < <(list_serial_ports)

    if [ ${#ports[@]} -eq 0 ]; then
        if [ -n "$NO_INTERACTIVE" ]; then
            die "No USB serial device found. Connect the Hydrom and set HYDROM_PORT=/dev/... to specify the port."
        fi
        warn "No USB serial device found."
        printf "\n"
        printf "  Make sure the Hydrom is connected via USB, then try again.\n"
        printf "  You can also type the port path manually.\n\n"
        local reply
        reply=$(ask "Press Enter to scan again, or type a port (e.g. /dev/ttyUSB0): ")
        if [ -n "$reply" ]; then
            PORT="$reply"
        else
            detect_port   # rescan
        fi
        return
    fi

    if [ ${#ports[@]} -eq 1 ]; then
        PORT="${ports[0]}"
        success "Device found at $PORT"
    else
        printf "\n  Multiple USB serial ports detected:\n\n"
        local i=1
        for p in "${ports[@]}"; do
            printf "    [%d]  %s\n" "$i" "$p"
            ((i++)) || true
        done
        printf "\n"
        local choice
        choice=$(ask "Enter the number of your Hydrom port [1]: " "1")
        PORT="${ports[$((choice - 1))]}"
        info "Using port: $PORT"
    fi
}

# ── Download latest GitHub release ───────────────────────────────────────────
fetch_release() {
    WORK_DIR=$(mktemp -d)
    info "Fetching latest release from GitHub..."

    local release_json
    release_json=$(curl -fsSL --retry 3 --retry-delay 2 \
        -H "Accept: application/vnd.github+json" \
        "$RELEASES_API" 2>/dev/null) \
        || die "Could not reach GitHub. Check your internet connection and try again."

    local tag
    tag=$("$PYTHON" -c \
        "import sys,json; d=json.load(sys.stdin); print(d['tag_name'])" \
        <<< "$release_json" 2>/dev/null) \
        || die "No published release found at https://github.com/${REPO}/releases"

    success "Latest release: $tag"
    info "Downloading firmware files..."

    local base_url="${DOWNLOAD_BASE}/${tag}"
    local i=0
    for file in "${FILES[@]}"; do
        printf "    ${DIM}%-35s${NC}" "${file}"
        local dest="${WORK_DIR}/${file}"
        local url="${base_url}/${file}"
        local ok=0

        for attempt in 1 2 3; do
            if curl -fsSL --retry 2 --retry-delay 2 -o "$dest" "$url" 2>/dev/null; then
                ok=1
                break
            fi
            [ $attempt -lt 3 ] && printf " retry %d/3..." "$((attempt + 1))" || true
        done

        if [ $ok -eq 1 ]; then
            local size
            size=$(wc -c < "$dest" | tr -d ' ')
            printf "${GREEN}  ✔  %s bytes${NC}\n" "$size"
        else
            printf "${RED}  FAILED${NC}\n"
            die "Could not download ${file} from ${url}"
        fi
        ((i++)) || true
    done

    success "All 5 firmware files downloaded"
}

# ── Flash ─────────────────────────────────────────────────────────────────────
do_flash() {
    # Build the write_flash argument list
    local flash_args=(
        "--chip"    "$CHIP"
        "--port"    "$PORT"
        "--baud"    "$FLASH_BAUD"
        "--before"  "default_reset"
        "--after"   "hard_reset"
        "write_flash" "-z"
        "--flash_mode" "$FLASH_MODE"
        "--flash_freq" "$FLASH_FREQ"
        "--flash_size" "detect"
    )
    for i in "${!FILES[@]}"; do
        flash_args+=("${ADDRS[$i]}" "${WORK_DIR}/${FILES[$i]}")
    done

    printf "\n"
    printf "  ${BOLD}Flash parameters${NC}\n"
    printf "    Port  : %s\n" "$PORT"
    printf "    Baud  : %s\n" "$FLASH_BAUD"
    printf "    Chip  : %s\n\n" "$CHIP"

    local attempt
    for attempt in 1 2 3; do
        press_enter "Hold the BOOT button on the Hydrom, then press Enter to flash..."
        printf "\n"

        if "${ESPTOOL_CMD[@]}" "${flash_args[@]}"; then
            return 0
        fi

        if [ $attempt -lt 3 ]; then
            printf "\n"
            warn "Flash attempt ${attempt}/3 failed. Common fixes:"
            printf "\n"
            printf "    • Hold BOOT before pressing Enter, release after 'Connecting...'\n"
            printf "    • Try a different USB cable (must support data, not charge-only)\n"
            printf "    • Try a different USB port on your computer\n"
            if [ "$OS" = "linux" ]; then
                printf "    • Run:  sudo chmod a+rw %s\n" "$PORT"
            fi
            printf "\n"
        fi
    done

    error "Flashing failed after 3 attempts."
    printf "\n"
    printf "  Additional troubleshooting:\n"
    printf "    • Retry with a lower baud rate:\n"
    printf "        HYDROM_BAUD=115200 bash install.sh\n"
    if [ "$OS" = "macos" ]; then
        printf "    • Ensure the CP2102 driver is installed:\n"
        printf "        https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers\n"
    else
        printf "    • Add yourself to the dialout group:\n"
        printf "        sudo usermod -aG dialout \$USER  (then log out and back in)\n"
    fi
    printf "    • Specify the port manually:\n"
    printf "        HYDROM_PORT=%s HYDROM_BAUD=115200 bash install.sh\n" "$PORT"
    exit 1
}

# ── Entry point ───────────────────────────────────────────────────────────────
main() {
    printf "\n"
    printf "${BOLD}${CYAN}╔══════════════════════════════════════════╗${NC}\n"
    printf "${BOLD}${CYAN}║       Hydrom  Firmware  Installer        ║${NC}\n"
    printf "${BOLD}${CYAN}╚══════════════════════════════════════════╝${NC}\n\n"

    detect_os

    header "Step 1 / 4  —  Dependencies"
    ensure_python
    ensure_esptool

    header "Step 2 / 4  —  Detect Device"
    detect_port

    header "Step 3 / 4  —  Download Firmware"
    fetch_release

    header "Step 4 / 4  —  Flash"
    do_flash

    printf "\n"
    printf "${BOLD}${GREEN}╔══════════════════════════════════════════╗${NC}\n"
    printf "${BOLD}${GREEN}║    ✔   Hydrom flashed successfully!      ║${NC}\n"
    printf "${BOLD}${GREEN}╚══════════════════════════════════════════╝${NC}\n\n"
    printf "  The device will restart automatically.\n"
    printf "  Connect to the Wi-Fi network named ${BOLD}Hydrom-XXXX${NC} to configure it.\n\n"
}

main "$@"
