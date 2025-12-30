param(
    [switch]$Clean,
    [string]$ComPort = "COM17",
    [int]$Baud = 9600,
    [string]$PicotoolPath = "",
    [string]$Uf2Path = ""
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$here/config.ps1"

$repoRoot = Resolve-Path "$here/.."
$buildScript = Join-Path $here "build.ps1"
$flashScript = Join-Path $here "flash_via_serial_bootsel.ps1"

# Run build
$buildArgs = @()
if ($Clean) { $buildArgs += "-Clean" }
& pwsh -NoProfile -File $buildScript @buildArgs

# Flash using serial BOOTSEL trigger + picotool
$flashArgs = @(
    "-ComPort", $ComPort,
    "-Baud", $Baud
)
if ($PicotoolPath) { $flashArgs += @("-PicotoolPath", $PicotoolPath) }
if ($Uf2Path) { $flashArgs += @("-Uf2Path", $Uf2Path) }
& pwsh -NoProfile -File $flashScript @flashArgs
