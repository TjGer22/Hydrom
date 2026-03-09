# =============================================================================
#  Hydrom Firmware Installer -- Windows (PowerShell 5.1+)
# =============================================================================
#  One-line install (run in PowerShell):
#
#    powershell -ExecutionPolicy Bypass -Command "irm https://raw.githubusercontent.com/TjGer22/Hydrom/main/firmware/Flasher/install.ps1 | iex"
#
#  Optional overrides (set before running):
#    $env:HYDROM_PORT = "COM5"      # skip port detection
#    $env:HYDROM_BAUD = "115200"    # lower baud if connection fails
# =============================================================================

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

# -- Constants -----------------------------------------------------------------
$Repo          = "TjGer22/Hydrom"
$Chip          = "esp32"
$FlashMode     = "dio"
$FlashFreq     = "40m"
$FlashBaud     = if ($env:HYDROM_BAUD)     { [int]$env:HYDROM_BAUD }    else { 460800 }
$ForcedPort    = if ($env:HYDROM_PORT)     { $env:HYDROM_PORT }         else { "" }
$NoInteractive = [bool]$env:HYDROM_NOINTERACTIVE  # skip all prompts (used by CI)

# Testing seams -- override via environment (leave unset in normal use)
$ApiUrl      = if ($env:HYDROM_API_URL)       { $env:HYDROM_API_URL }       else { "https://api.github.com/repos/$Repo/releases/latest" }
$DownloadBase = if ($env:HYDROM_DOWNLOAD_BASE) { $env:HYDROM_DOWNLOAD_BASE } else { "https://github.com/$Repo/releases/download" }

$Files = [ordered]@{
    "bootloader_dio_40m.bin" = "0x1000"
    "partitions.bin"         = "0x8000"
    "boot_app0.bin"          = "0xe000"
    "firmware.bin"           = "0x10000"
    "spiffs.bin"             = "0x5B0000"
}

# -- Output helpers -------------------------------------------------------------
function Write-Header  { param($t) Write-Host "`n-- $t $(('-' * [Math]::Max(1,40-$t.Length)))`n" -ForegroundColor Cyan }
function Write-Info    { param($t) Write-Host "  >  $t" -ForegroundColor Cyan }
function Write-Success { param($t) Write-Host "  v  $t" -ForegroundColor Green }
function Write-Warn    { param($t) Write-Host "  !  $t" -ForegroundColor Yellow }
function Write-Err     { param($t) Write-Host "  x  $t" -ForegroundColor Red }
function Stop-WithError { param($t) Write-Err $t; exit 1 }

# -- Read user input safely -----------------------------------------------------
function Read-Answer {
    param([string]$Prompt, [string]$Default = "")
    Write-Host "  ?  $Prompt" -ForegroundColor Yellow -NoNewline
    $reply = Read-Host
    if ($reply -eq "") { return $Default }
    return $reply
}

function Wait-Enter {
    param([string]$Prompt)
    if ($NoInteractive) { return }
    Write-Host "  >  $Prompt" -ForegroundColor Yellow -NoNewline
    $null = Read-Host
}

# -- Cleanup on exit -----------------------------------------------------------
$WorkDir = $null

function Remove-WorkDir {
    if ($WorkDir -and (Test-Path $WorkDir)) {
        Remove-Item $WorkDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# -- Python detection & installation -------------------------------------------
$Python = $null

function Find-Python {
    foreach ($cmd in @("python", "python3", "py")) {
        $found = Get-Command $cmd -ErrorAction SilentlyContinue
        if ($found) {
            $ver = & $cmd -c "import sys; print(sys.version_info.major)" 2>$null
            if ($LASTEXITCODE -eq 0 -and [int]$ver -ge 3) {
                return $cmd
            }
        }
    }
    return $null
}

function Install-PythonWindows {
    Write-Warn "Python 3 not found -- attempting to install..."

    # Option 1: winget (available on Windows 10 1709+ / Windows 11)
    if (Get-Command winget -ErrorAction SilentlyContinue) {
        Write-Info "Installing Python 3.12 via winget..."
        try {
            winget install --id Python.Python.3.12 `
                --accept-package-agreements --accept-source-agreements --silent
            # Refresh PATH from registry
            $env:PATH = [Environment]::GetEnvironmentVariable("PATH", "Machine") + ";" +
                        [Environment]::GetEnvironmentVariable("PATH", "User")
            return
        } catch {
            Write-Warn "winget install failed -- trying direct download."
        }
    }

    # Option 2: Direct installer download
    Write-Info "Downloading Python 3.12 installer..."
    $arch    = if ([Environment]::Is64BitOperatingSystem) { "amd64" } else { "win32" }
    $pyUrl   = "https://www.python.org/ftp/python/3.12.4/python-3.12.4-$arch.exe"
    $pySetup = Join-Path $env:TEMP "python_setup.exe"

    try {
        Invoke-WebRequest -Uri $pyUrl -OutFile $pySetup -UseBasicParsing
    } catch {
        Stop-WithError "Could not download Python installer. Install manually from https://www.python.org/downloads/ and re-run this script."
    }

    Write-Info "Running installer silently (adds Python to PATH)..."
    $proc = Start-Process -FilePath $pySetup `
        -ArgumentList "/quiet", "InstallAllUsers=0", "PrependPath=1", "Include_pip=1" `
        -Wait -PassThru
    Remove-Item $pySetup -Force -ErrorAction SilentlyContinue

    if ($proc.ExitCode -ne 0) {
        Stop-WithError "Python installer exited with code $($proc.ExitCode). Please install manually from https://www.python.org/downloads/"
    }

    # Reload PATH from registry
    $env:PATH = [Environment]::GetEnvironmentVariable("PATH", "Machine") + ";" +
                [Environment]::GetEnvironmentVariable("PATH", "User")
}

function Ensure-Python {
    $script:Python = Find-Python
    if (-not $script:Python) {
        Install-PythonWindows
        $script:Python = Find-Python
        if (-not $script:Python) {
            Stop-WithError "Python 3 is still not found after installation. Please install it manually from https://www.python.org/downloads/ (tick 'Add Python to PATH') and re-run this script."
        }
    }
    $ver = & $script:Python --version 2>&1
    Write-Success "Python: $ver"
}

# -- esptool detection & installation ------------------------------------------
$EsptoolCmd = $null   # will be a scriptblock for safe invocation

function Find-Esptool {
    # Prefer 'esptool' over the deprecated 'esptool.py' wrapper
    foreach ($cmd in @("esptool", "esptool.py")) {
        if (Get-Command $cmd -ErrorAction SilentlyContinue) {
            $script:EsptoolCmd = { param($a) & $cmd @a }
            return $true
        }
    }
    # Try python -m esptool (works when installed in the active venv or user site)
    $test = & $script:Python -m esptool version 2>&1
    if ($LASTEXITCODE -eq 0) {
        $py = $script:Python
        $script:EsptoolCmd = { param($a) & $py -m esptool @a }
        return $true
    }
    return $false
}

function Ensure-Esptool {
    if (Find-Esptool) {
        $ver = (& $script:Python -m esptool version 2>&1 | Select-Object -First 1)
        Write-Success "esptool: $ver"
        return
    }

    $venvDir = Join-Path $env:USERPROFILE ".hydrom_esptool_venv"

    # Remove any previous venv so we always get the latest esptool
    if (Test-Path $venvDir) {
        Write-Info "Removing old esptool venv ($venvDir)..."
        Remove-Item -Recurse -Force $venvDir
    }
    Write-Info "Installing esptool into venv ($venvDir)..."

    & $script:Python -m venv $venvDir
    if ($LASTEXITCODE -ne 0) {
        Stop-WithError "Could not create a Python venv. Make sure your Python installation supports venv."
    }

    $venvPython = Join-Path $venvDir "Scripts\python.exe"

    # Upgrade pip first to suppress version notices
    & $venvPython -m pip install --quiet --disable-pip-version-check --upgrade pip | Out-Null

    & $venvPython -m pip install --quiet --disable-pip-version-check esptool
    if ($LASTEXITCODE -ne 0) {
        Stop-WithError "esptool installation failed inside venv."
    }

    # Add venv Scripts dir to PATH and switch Python to the venv
    $venvScripts = Join-Path $venvDir "Scripts"
    $env:PATH = "$venvScripts;$env:PATH"
    $script:Python = $venvPython

    if (Find-Esptool) {
        $ver = (& $script:Python -m esptool version 2>&1 | Select-Object -First 1)
        Write-Success "esptool installed: $ver"
    } else {
        Stop-WithError "esptool installed into venv but still not found. Please report this issue."
    }
}

# -- CP2102 driver check --------------------------------------------------------
function Test-DriverInstalled {
    $driver = Get-WmiObject Win32_PnPEntity -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match "CP210|Silicon Labs" } |
        Select-Object -First 1
    return ($null -ne $driver)
}

# -- Serial port detection -----------------------------------------------------
$Port = $null

function Find-HydromPorts {
    $ports = @()

    # 1. Look for known ESP32 USB-serial chips by device name
    $candidates = Get-WmiObject Win32_PnPEntity -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match "CP210|CH340|CH341|FTDI|USB Serial|Silicon Labs" }

    foreach ($c in $candidates) {
        if ($c.Name -match "COM(\d+)") {
            $ports += "COM$($Matches[1])"
        }
    }

    # 2. Fallback: all serial ports with USB in the description
    if ($ports.Count -eq 0) {
        $allPorts = Get-WmiObject Win32_SerialPort -ErrorAction SilentlyContinue
        foreach ($p in $allPorts) {
            if ($p.Description -match "USB") {
                $ports += $p.DeviceID
            }
        }
    }

    return $ports | Sort-Object -Unique
}

function Ensure-Port {
    if ($ForcedPort -ne "") {
        $script:Port = $ForcedPort
        Write-Info "Using port from environment: $($script:Port)"
        return
    }

    Write-Info "Scanning for connected Hydrom device..."
    $ports = Find-HydromPorts

    if ($ports.Count -eq 0) {
        if ($NoInteractive) {
            Stop-WithError "No USB serial device found. Connect the Hydrom and set HYDROM_PORT to specify the port manually."
        }
        Write-Warn "No USB serial device found."

        if (-not (Test-DriverInstalled)) {
            Write-Host ""
            Write-Warn "The CP2102 USB driver does not appear to be installed."
            Write-Host "  Download and install it from:" -ForegroundColor Yellow
            Write-Host "    https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers" -ForegroundColor Cyan
            Write-Host "  After installing, reconnect the Hydrom and press Enter." -ForegroundColor Yellow
        }

        Write-Host ""
        $reply = Read-Answer "Press Enter to scan again, or type a port (e.g. COM3): "
        if ($reply -ne "") {
            $script:Port = $reply
        } else {
            Ensure-Port   # rescan
        }
        return
    }

    if ($ports.Count -eq 1) {
        $script:Port = $ports[0]
        Write-Success "Device found at $($script:Port)"
    } else {
        Write-Host ""
        Write-Host "  Multiple USB serial ports detected:" -ForegroundColor White
        for ($i = 0; $i -lt $ports.Count; $i++) {
            Write-Host "    [$($i + 1)]  $($ports[$i])"
        }
        Write-Host ""
        $choice = Read-Answer "Enter the number of your Hydrom port [1]: " "1"
        $idx = [int]$choice - 1
        if ($idx -lt 0 -or $idx -ge $ports.Count) { $idx = 0 }
        $script:Port = $ports[$idx]
        Write-Info "Using port: $($script:Port)"
    }
}

# -- Download latest release ---------------------------------------------------
function Get-LatestRelease {
    $tmp = [System.IO.Path]::GetTempPath()
    $script:WorkDir = Join-Path $tmp "hydrom_flash_$(Get-Random)"
    New-Item -ItemType Directory -Path $script:WorkDir | Out-Null

    Write-Info "Fetching latest release from GitHub..."
    try {
        $release = Invoke-RestMethod -Uri $ApiUrl -UseBasicParsing -ErrorAction Stop
    } catch {
        Stop-WithError "Could not reach GitHub API. Check your internet connection.`n  Error: $_"
    }

    $tag = $release.tag_name
    if (-not $tag) {
        Stop-WithError "No published release found at https://github.com/$Repo/releases"
    }
    Write-Success "Latest release: $tag"

    Write-Info "Downloading firmware files..."
    $baseUrl = "$DownloadBase/$tag"

    foreach ($file in $Files.Keys) {
        $url  = "$baseUrl/$file"
        $dest = Join-Path $script:WorkDir $file
        Write-Host "    $($file.PadRight(35))" -NoNewline

        $ok = $false
        for ($attempt = 1; $attempt -le 3; $attempt++) {
            try {
                Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing -ErrorAction Stop
                $ok = $true
                break
            } catch {
                if ($attempt -lt 3) {
                    Write-Host " retry $($attempt+1)/3..." -NoNewline
                    Start-Sleep -Seconds 2
                }
            }
        }

        if ($ok) {
            $size = (Get-Item $dest).Length
            Write-Host "  OK  ($size bytes)" -ForegroundColor Green
        } else {
            Write-Host "  FAILED" -ForegroundColor Red
            Stop-WithError "Could not download $file from $url"
        }
    }

    Write-Success "All 5 firmware files downloaded"
}

# -- Flash ---------------------------------------------------------------------
function Start-Flash {
    # Build argument array for esptool
    $args = @(
        "--chip",   $Chip,
        "--port",   $script:Port,
        "--baud",   $FlashBaud,
        "--before", "default_reset",
        "--after",  "hard_reset",
        "write_flash", "-z",
        "--flash_mode", $FlashMode,
        "--flash_freq", $FlashFreq,
        "--flash_size", "detect"
    )
    foreach ($file in $Files.Keys) {
        $args += $Files[$file]
        $args += (Join-Path $script:WorkDir $file)
    }

    Write-Host ""
    Write-Host "  Flash parameters" -ForegroundColor White
    Write-Host "    Port  : $($script:Port)"
    Write-Host "    Baud  : $FlashBaud"
    Write-Host "    Chip  : $Chip"
    Write-Host ""

    $flashSuccess = $false
    for ($attempt = 1; $attempt -le 3; $attempt++) {
        Wait-Enter "Hold the BOOT button on the Hydrom, then press Enter to flash..."
        Write-Host ""

        & $script:Python -m esptool @args
        if ($LASTEXITCODE -eq 0) {
            $flashSuccess = $true
            break
        }

        if ($attempt -lt 3) {
            Write-Host ""
            Write-Warn "Flash attempt $attempt/3 failed. Common fixes:"
            Write-Host ""
            Write-Host "    * Hold BOOT before pressing Enter, release after you see 'Connecting...'" -ForegroundColor Yellow
            Write-Host "    * Use a USB cable that supports data transfer (not charge-only)" -ForegroundColor Yellow
            Write-Host "    * Try a different USB port on your computer" -ForegroundColor Yellow
            Write-Host "    * Verify the correct COM port is selected ($($script:Port))" -ForegroundColor Yellow
            Write-Host ""
        }
    }

    if (-not $flashSuccess) {
        Write-Host ""
        Write-Err "Flashing failed after 3 attempts."
        Write-Host ""
        Write-Host "  Additional troubleshooting:" -ForegroundColor White
        Write-Host "    * Install or reinstall the CP2102 driver:"
        Write-Host "      https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers" -ForegroundColor Cyan
        Write-Host "    * Try a lower baud rate:"
        Write-Host "      `$env:HYDROM_BAUD='115200'; irm ... | iex" -ForegroundColor Cyan
        Write-Host "    * Specify the port manually:"
        Write-Host "      `$env:HYDROM_PORT='$($script:Port)'; irm ... | iex" -ForegroundColor Cyan
        exit 1
    }
}

# -- Entry point ---------------------------------------------------------------
try {
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "      Hydrom  Firmware  Installer          " -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host ""

    Write-Header "Step 1 / 4  --  Dependencies"
    Ensure-Python
    Ensure-Esptool

    Write-Header "Step 2 / 4  --  Detect Device"
    Ensure-Port

    Write-Header "Step 3 / 4  --  Download Firmware"
    Get-LatestRelease

    Write-Header "Step 4 / 4  --  Flash"
    Start-Flash

    Write-Host ""
    Write-Host "============================================" -ForegroundColor Green
    Write-Host "   OK   Hydrom flashed successfully!       " -ForegroundColor Green
    Write-Host "============================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "  The device will restart automatically."
    Write-Host "  Connect to the Wi-Fi network named 'Hydrom-XXXX' to configure it."
    Write-Host ""
} finally {
    Remove-WorkDir
}
