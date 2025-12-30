param(
    [Parameter(Mandatory=$true)][string]$ComPort,
    [string]$Uf2Path,
    [string]$PicotoolPath,
    [int]$TimeoutSec = 15,
    [int]$Baud = 115200
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$here/config.ps1"

$repoRoot = Resolve-Path "$here/.."
$defaultUf2 = Join-Path $repoRoot "$($config.BUILD_DIR)/examples/baremetal/$($config.TARGET).uf2"
$uf2 = if ($Uf2Path) { Resolve-Path $Uf2Path } else { $defaultUf2 }
if (-not (Test-Path $uf2)) { throw "UF2 not found: $uf2. Build first or pass -Uf2Path." }

$picotoolCandidates = @()
if ($PicotoolPath) {
    $picotoolCandidates += (Resolve-Path $PicotoolPath)
} else {
    $picotoolCandidates += @( 
        Join-Path $repoRoot "build/picotool-usb-vs/Release/picotool.exe" 
        Join-Path $repoRoot "$($config.BUILD_DIR)/_deps/picotool/picotool.exe"
    )
}
$picotool = $picotoolCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $picotool) {
    $msg = "picotool not found. Provide -PicotoolPath or build one (USB-enabled preferred) at: `n" + ($picotoolCandidates -join "`n")
    throw $msg
}

function Assert-PicotoolUsbSupport {
    param($picotool)
    $help = & $picotool help load 2>&1
    if ($LASTEXITCODE -ne 0 -or ($help -join "`n") -match "Unknown command") {
        throw "The picotool at $picotool was built without USB support. Rebuild with -DPICOTOOL_ENABLE_USBD=1 (example: cmake -S deps/picotool -B build/picotool -G Ninja -DPICOTOOL_BUILD_CORE_ONLY=1 -DPICOTOOL_ENABLE_USBD=1) and pass -PicotoolPath to the USB-enabled binary."
    }
}

# Send BOOTSEL trigger over CDC/UART. The firmware must listen for "BOOTSEL" + Enter.
function Invoke-BootselTrigger {
    param($port, $baud, $retries = 5)
    for ($i = 0; $i -lt $retries; $i++) {
        $sp = New-Object System.IO.Ports.SerialPort $port,$baud,'None',8,'One'
        $sp.NewLine = "`n"
        $sp.ReadTimeout = 500
        $sp.WriteTimeout = 500
        $sp.Handshake = [System.IO.Ports.Handshake]::None
        try {
            $sp.Open()
            $sp.DtrEnable = $false
            $sp.RtsEnable = $false
            Start-Sleep -Milliseconds 50
            $sp.DtrEnable = $true
            $sp.RtsEnable = $true
            Start-Sleep -Milliseconds 50
            $sp.DiscardInBuffer()
            $sp.DiscardOutBuffer()
            $sp.WriteLine("BOOTSEL")
            Start-Sleep -Milliseconds 400
            return
        } catch {
            Start-Sleep -Milliseconds 200
        } finally {
            if ($sp.IsOpen) { $sp.Close() }
        }
    }
    throw "Failed to send BOOTSEL after $retries attempts on $port"
}

function Wait-ForRomDevice {
    param($picotool, $timeoutSec)
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $timeoutSec) {
        $devs = & $picotool info -b 2>$null
        if ($LASTEXITCODE -eq 0 -and $devs) { return $true }
        Start-Sleep -Milliseconds 200
    }
    return $false
}

function Ensure-RomDevice {
    param($picotool, $timeoutSec)
    if (Wait-ForRomDevice -picotool $picotool -timeoutSec $timeoutSec) { return $true }
    Write-Host "Forcing reboot into BOOTSEL via picotool..." -ForegroundColor Yellow
    try { & $picotool reboot -f -u } catch {}
    return Wait-ForRomDevice -picotool $picotool -timeoutSec $timeoutSec
}

Write-Host "Triggering BOOTSEL over $ComPort..." -ForegroundColor Cyan
Invoke-BootselTrigger -port $ComPort -baud $Baud
Write-Host "Forcing reboot into BOOTSEL via picotool (no wait)..." -ForegroundColor Cyan
try { & $picotool reboot -f -u } catch { Write-Host "picotool reboot warning: $($_.Exception.Message)" -ForegroundColor Yellow }
Start-Sleep -Milliseconds 600

Write-Host "Loading UF2 via picotool..." -ForegroundColor Cyan
Assert-PicotoolUsbSupport -picotool $picotool
& $picotool load --family rp2350-arm-s $uf2
Write-Host "Rebooting..." -ForegroundColor Cyan
& $picotool reboot -f
Write-Host "Done." -ForegroundColor Green
