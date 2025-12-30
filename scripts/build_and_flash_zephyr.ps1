param(
    [switch]$Clean,
    [string]$Board = $null,
    [string]$ComPort,
    [int]$Baud = 115200,
    [string]$PicotoolPath,
    [string]$Uf2Path
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$here/config.ps1"

$repoRoot = Resolve-Path "$here/.."
$buildRoot = Join-Path $repoRoot "build/zephyr"
$buildDir = Join-Path $buildRoot "zephyr"

if (-not $Board) { $Board = $config.PICO_BOARD }

# Build
$buildArgs = @{
    Clean = $Clean
    Board = $Board
}
& "$here/build_zephyr.ps1" @buildArgs

# Determine UF2 path
$defaultUf2 = Join-Path $buildDir "zephyr/zephyr.uf2"
$uf2 = if ($Uf2Path) { Resolve-Path $Uf2Path } else { $defaultUf2 }
if (-not (Test-Path $uf2)) {
    throw "UF2 not found at $uf2. Ensure the board/runner produces a UF2 or pass -Uf2Path."
}

if (-not $ComPort) {
    throw "Provide -ComPort for serial BOOTSEL triggering."
}

# Flash via existing serial BOOTSEL helper
& "$here/flash_via_serial_bootsel.ps1" -ComPort $ComPort -Baud $Baud -PicotoolPath $PicotoolPath -Uf2Path $uf2
