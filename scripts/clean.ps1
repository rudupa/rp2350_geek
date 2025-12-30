$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$here/config.ps1"

$repoRoot = Resolve-Path "$here/.."
$buildDir = Join-Path $repoRoot $config.BUILD_DIR

if (Test-Path $buildDir) {
    Write-Host "Removing $buildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
} else {
    Write-Host "Nothing to clean." -ForegroundColor Green
}
