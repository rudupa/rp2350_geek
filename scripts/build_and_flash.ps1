param(
    [switch]$Clean,
    [ValidateSet("rp2350", "rp2350-riscv")]
    [string]$Platform = "rp2350",
    [string]$ComPort = "COM17",
    [int]$Baud = 9600,
    [string]$PicotoolPath = "",
    [string]$Uf2Path = ""
)

$ErrorActionPreference = "Stop"
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$here/config.ps1"

$repoRoot = Resolve-Path "$here/.."
$buildScript = Join-Path $here "build.ps1"
$flashScript = Join-Path $here "flash_via_serial_bootsel.ps1"

# Run build
$buildArgs = @()
if ($Clean) { $buildArgs += "-Clean" }
if ($Platform) { $buildArgs += @("-Platform", $Platform) }
& pwsh -NoProfile -File $buildScript @buildArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Flash using serial BOOTSEL trigger + picotool
$flashArgs = @(
    "-ComPort", $ComPort,
    "-Baud", $Baud
)
if ($PicotoolPath) { $flashArgs += @("-PicotoolPath", $PicotoolPath) }
if ($Uf2Path) { $flashArgs += @("-Uf2Path", $Uf2Path) }
& pwsh -NoProfile -File $flashScript @flashArgs
