param(
    [switch]$Clean,
    [string]$Board = $null
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$here/config.ps1"

$repoRoot = Resolve-Path "$here/.."
$zephyrApp = Join-Path $repoRoot "zephyr"
$buildRoot = Join-Path $repoRoot "build/zephyr"
$buildDir = Join-Path $buildRoot "zephyr"

if (-not $Board) { $Board = $config.PICO_BOARD }

if ($Clean -and (Test-Path $buildRoot)) {
    Remove-Item -Recurse -Force $buildRoot
}

if (-not (Get-Command west -ErrorAction SilentlyContinue)) {
    throw "west not found in PATH. Install Zephyr SDK/West and ensure 'west' is available."
}

Write-Host "Configuring Zephyr app (board=$Board)..." -ForegroundColor Cyan
& west build -b $Board -d $buildDir $zephyrApp
if ($LASTEXITCODE -ne 0) { throw "west build failed with exit code $LASTEXITCODE" }

Write-Host "Done. Zephyr build artifacts in $buildDir/zephyr" -ForegroundColor Green
