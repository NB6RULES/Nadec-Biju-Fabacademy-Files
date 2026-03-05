# Xiao ESP32-C6 Wokwi Test Script
# Build firmware and run Wokwi CLI simulation

param(
    [int]$Timeout = 30000,
    [switch]$SkipBuild,
    [switch]$Verbose
)

function Write-Step {
    param([string]$Message)
    Write-Host "`n==> $Message" -ForegroundColor Cyan
}

function Write-Ok {
    param([string]$Message)
    Write-Host "[OK] $Message" -ForegroundColor Green
}

function Write-Fail {
    param([string]$Message)
    Write-Host "[ERR] $Message" -ForegroundColor Red
}

Write-Host "Xiao ESP32-C6 Wokwi test" -ForegroundColor White

Write-Step "Checking prerequisites"

$UsePy313PlatformIO = $false
try {
    py -3.13 -m platformio --version | Out-Null
    if ($LASTEXITCODE -eq 0) {
        $UsePy313PlatformIO = $true
    }
} catch {}

$pioExists = Get-Command pio -ErrorAction SilentlyContinue
if (-not $UsePy313PlatformIO -and -not $pioExists) {
    Write-Fail "PlatformIO not found"
    Write-Host "Install: py -3.13 -m pip install --user platformio" -ForegroundColor Yellow
    exit 1
}

function Invoke-Pio {
    param([string[]]$Args)
    if ($UsePy313PlatformIO) {
        & py -3.13 -m platformio @Args
    } else {
        & pio @Args
    }
}

if ($UsePy313PlatformIO) {
    Write-Ok "PlatformIO via Python 3.13"
} else {
    Write-Ok "PlatformIO via pio on PATH"
}

$wokwiCmd = Get-Command wokwi-cli -ErrorAction SilentlyContinue
$wokwiPath = if ($wokwiCmd) { $wokwiCmd.Source } else { "$env:USERPROFILE\.wokwi\bin\wokwi-cli.exe" }
if (-not (Test-Path $wokwiPath)) {
    Write-Fail "wokwi-cli not found"
    Write-Host "Install: iwr https://wokwi.com/ci/install.ps1 -useb | iex" -ForegroundColor Yellow
    exit 1
}
Write-Ok "wokwi-cli found at $wokwiPath"

if (-not $env:WOKWI_CLI_TOKEN) {
    Write-Fail "WOKWI_CLI_TOKEN is not set"
    Write-Host "Get token: https://wokwi.com/dashboard/ci" -ForegroundColor Yellow
    Write-Host "Current shell: `$env:WOKWI_CLI_TOKEN = 'your_token_here'" -ForegroundColor Yellow
    exit 1
}
Write-Ok "WOKWI_CLI_TOKEN is set"

if (-not (Test-Path "wokwi.toml")) {
    Write-Fail "wokwi.toml not found"
    exit 1
}
if (-not (Test-Path "diagram.json")) {
    Write-Fail "diagram.json not found"
    exit 1
}
Write-Ok "Project files found"

if (-not $SkipBuild) {
    Write-Step "Building firmware"
    if ($Verbose) {
        Invoke-Pio @("run", "-e", "seeed_xiao_esp32c6")
    } else {
        Invoke-Pio @("run", "-e", "seeed_xiao_esp32c6") | Out-Null
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Fail "Build failed"
        exit 1
    }
    Write-Ok "Build completed"
} else {
    Write-Host "Skipping build" -ForegroundColor Yellow
}

$firmwareBin = ".\.pio\build\seeed_xiao_esp32c6\firmware.bin"
$firmwareElf = ".\.pio\build\seeed_xiao_esp32c6\firmware.elf"
if (-not (Test-Path $firmwareBin) -or -not (Test-Path $firmwareElf)) {
    Write-Fail "Firmware artifacts not found"
    exit 1
}
Write-Ok "Firmware artifacts found"

Write-Step "Running simulation (timeout: $Timeout ms)"
$serialLog = "serial-output.txt"

if ($Verbose) {
    & $wokwiPath --timeout $Timeout --serial-log-file $serialLog .
} else {
    & $wokwiPath --timeout $Timeout --serial-log-file $serialLog . | Out-Null
}

$exitCode = $LASTEXITCODE
if ($exitCode -ne 0 -and $exitCode -ne 100) {
    Write-Fail "Simulation failed (exit code $exitCode)"
    exit $exitCode
}
Write-Ok "Simulation finished"

if (Test-Path $serialLog) {
    Write-Step "Serial output"
    Get-Content $serialLog
}

Write-Host "`nDone." -ForegroundColor Green
exit 0
