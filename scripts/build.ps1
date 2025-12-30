param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$here/config.ps1"

$repoRoot = Resolve-Path "$here/.."
$buildDir = Join-Path $repoRoot $config.BUILD_DIR

# Helper: run inside a vcvars-enabled cmd when available so picotool can build with MSVC.
function Invoke-WithVcvars {
    param(
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$CommandLine
    )

    if (Test-Path $config.VCVARS_BAT) {
        $cmdLine = 'call "{0}" && {1}' -f $config.VCVARS_BAT, ($CommandLine -join ' ')
        Write-Host "[vcvars cmd] $cmdLine" -ForegroundColor DarkGray
        & cmd /c $cmdLine
    } else {
        & $CommandLine[0] @($CommandLine[1..($CommandLine.Length - 1)])
    }
}

if ($Clean -and (Test-Path $buildDir)) {
    Remove-Item -Recurse -Force $buildDir
}

$toolchainBin = "$($config.TOOLCHAIN_DIR)/bin"
$env:PATH = "$toolchainBin;${env:PATH}"
$cmakeArgs = @(
    "-S", $repoRoot,
    "-B", $buildDir,
    "-G", $config.CMAKE_GENERATOR,
    "-DCMAKE_SYSTEM_NAME=$($config.CMAKE_SYSTEM_NAME)",
    "-DCMAKE_BUILD_TYPE=$($config.CMAKE_BUILD_TYPE)",
    "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
    "-DPICO_SDK_PATH=$($config.PICO_SDK_PATH)",
    "-DPICO_PLATFORM=$($config.PICO_PLATFORM)",
    "-DPICO_BOARD=$($config.PICO_BOARD)",
    "-DPYTHON_EXECUTABLE=$($config.PYTHON_EXECUTABLE)",
    "-DPython3_EXECUTABLE=$($config.PYTHON_EXECUTABLE)",
    "-DCMAKE_C_COMPILER=$toolchainBin/arm-none-eabi-gcc.exe",
    "-DCMAKE_CXX_COMPILER=$toolchainBin/arm-none-eabi-g++.exe",
    "-DCMAKE_ASM_COMPILER=$toolchainBin/arm-none-eabi-gcc.exe"
)

Write-Host "Configuring..." -ForegroundColor Cyan
Invoke-WithVcvars cmake @cmakeArgs

Write-Host "Building target $($config.TARGET)..." -ForegroundColor Cyan
Invoke-WithVcvars cmake --build $buildDir -t $config.TARGET

Write-Host "Done. Artifacts in $buildDir" -ForegroundColor Green
