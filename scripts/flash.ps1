param(
    [ValidateSet('mass','picotool')][string]$Method = 'mass',
    [string]$VolumeLabel = "RP2350",
    [string]$Uf2Path,
    [string]$PicotoolPath
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$here/config.ps1"

$repoRoot = Resolve-Path "$here/.."
$defaultUf2 = Join-Path $repoRoot "$($config.BUILD_DIR)/examples/baremetal/$($config.TARGET).uf2"
$uf2 = if ($Uf2Path) { Resolve-Path $Uf2Path } else { $defaultUf2 }

if (-not (Test-Path $uf2)) {
    Write-Error "UF2 not found: $uf2. Build first or pass -Uf2Path."
}

function Flash-MassStorage {
    param($VolumeLabel, $uf2)
    $volume = Get-Volume -FileSystemLabel $VolumeLabel -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $volume) {
        Write-Error "No drive with label '$VolumeLabel' found. Put the board in BOOTSEL mode (hold BOOTSEL, plug USB) and try again."
    }
    $destPath = Join-Path ("{0}:\" -f $volume.DriveLetter) (Split-Path $uf2 -Leaf)
    Write-Host "Flashing to $destPath" -ForegroundColor Cyan
    Copy-Item -LiteralPath $uf2 -Destination $destPath -Force
    Write-Host "Flash complete." -ForegroundColor Green
}

function Flash-Picotool {
    param($picotool, $uf2)
    if (-not (Test-Path $picotool)) {
        Write-Error "picotool not found at $picotool. Pass -PicotoolPath or build to generate it."
    }
    $usbDevs = & $picotool list -q --family rp2350-arm-s 2>$null
    if (-not $usbDevs) {
        Write-Error "No RP2350 device in BOOTSEL/ROM over USB. Hold BOOTSEL, plug in, and try again."
    }
    Write-Host "Loading UF2 via picotool..." -ForegroundColor Cyan
    & $picotool load --family rp2350-arm-s --halt $uf2
    Write-Host "Rebooting..." -ForegroundColor Cyan
    & $picotool reboot
    Write-Host "Flash complete via picotool." -ForegroundColor Green
}

switch ($Method) {
    'mass' {
        Flash-MassStorage -VolumeLabel $VolumeLabel -uf2 $uf2
    }
    'picotool' {
        $picotool = if ($PicotoolPath) { Resolve-Path $PicotoolPath } else { Join-Path $repoRoot "$($config.BUILD_DIR)/_deps/picotool/picotool.exe" }
        Flash-Picotool -picotool $picotool -uf2 $uf2
    }
}
