param(
    [switch]$Clean,
    [ValidateSet("rp2350", "rp2350-riscv")]
    [string]$Platform = $null
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$here/config.ps1"

$repoRoot = Resolve-Path "$here/.."
$buildDir = Join-Path $repoRoot $config.BUILD_DIR

if (-not $Platform) { $Platform = $config.PICO_PLATFORM }

$usingArm = $Platform -eq "rp2350"

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

function Find-RiscvCompiler {
    param(
        [string[]]$Names,
        [string]$PreferredBin
    )

    foreach ($name in $Names) {
        if ($PreferredBin) {
            $candidate = Join-Path $PreferredBin "$name.exe"
            if (Test-Path $candidate) { return $candidate }
        }
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Path }
    }
    return $null
}

if ($Clean -and (Test-Path $buildDir)) {
    Remove-Item -Recurse -Force $buildDir
}

$toolchainBin = $null
if ($usingArm) {
    $toolchainBin = "$($config.TOOLCHAIN_DIR)/bin"
} else {
    if (-not $config.RISCV_TOOLCHAIN_DIR) {
        Write-Warning "RISCV_TOOLCHAIN_DIR is not set; relying on riscv toolchain in PATH"
    } else {
        $toolchainBin = "$($config.RISCV_TOOLCHAIN_DIR)/bin"
    }
}

if ($toolchainBin) { $env:PATH = "$toolchainBin;${env:PATH}" }

$cmakeCc = $null
$cmakeCxx = $null
$cmakeAsm = $null

$archFlags = ""
if ($usingArm) {
    $cmakeCc = "$toolchainBin/arm-none-eabi-gcc.exe"
    $cmakeCxx = "$toolchainBin/arm-none-eabi-g++.exe"
    $cmakeAsm = $cmakeCc
    $archFlags = "-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
} else {
    $gcc = Find-RiscvCompiler -Names @("riscv32-unknown-elf-gcc", "riscv-none-elf-gcc", "riscv64-unknown-elf-gcc") -PreferredBin $toolchainBin
    if (-not $gcc) {
        throw "RISC-V GCC not found. Set config.RISCV_TOOLCHAIN_DIR or ensure riscv*-gcc is on PATH."
    }
    $prefixDir = Split-Path $gcc -Parent
    $base = [System.IO.Path]::GetFileNameWithoutExtension($gcc)
    $basePrefix = $base -replace "gcc$", ""
    $gxx = Join-Path $prefixDir ($basePrefix + "g++.exe")
    if (-not (Test-Path $gxx)) { $gxx = $gcc -replace "gcc.exe$", "g++.exe" }
    $cmakeCc = $gcc
    $cmakeCxx = $gxx
    $cmakeAsm = $gcc
    $archFlags = "-march=rv32imc_zicsr_zifencei_zba_zbb_zbs_zbkb -mabi=ilp32"
}
$cmakeArgs = @(
    "-S", $repoRoot,
    "-B", $buildDir,
    "-G", $config.CMAKE_GENERATOR,
    "-DCMAKE_SYSTEM_NAME=$($config.CMAKE_SYSTEM_NAME)",
    "-DCMAKE_BUILD_TYPE=$($config.CMAKE_BUILD_TYPE)",
    "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
    "-DPICO_SDK_PATH=$($config.PICO_SDK_PATH)",
    "-DPICO_PLATFORM=$Platform",
    "-DPICO_BOARD=$($config.PICO_BOARD)",
    "-DPYTHON_EXECUTABLE=$($config.PYTHON_EXECUTABLE)",
    "-DPython3_EXECUTABLE=$($config.PYTHON_EXECUTABLE)"
)

if ($cmakeCc) {
    $cmakeArgs += @(
        "-DCMAKE_C_COMPILER=$cmakeCc",
        "-DCMAKE_CXX_COMPILER=$cmakeCxx",
        "-DCMAKE_ASM_COMPILER=$cmakeAsm"
    )
}

if ($archFlags) {
    $quotedFlags = '"' + $archFlags + '"'
    $cmakeArgs += @(
        "-DCMAKE_C_FLAGS=$quotedFlags",
        "-DCMAKE_CXX_FLAGS=$quotedFlags",
        "-DCMAKE_ASM_FLAGS=$quotedFlags"
    )
}

Write-Host "Configuring (platform=$Platform)..." -ForegroundColor Cyan
Invoke-WithVcvars cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }

Write-Host "Building target $($config.TARGET) (platform=$Platform)..." -ForegroundColor Cyan
Invoke-WithVcvars cmake --build $buildDir -t $config.TARGET
if ($LASTEXITCODE -ne 0) { throw "CMake build failed with exit code $LASTEXITCODE" }

Write-Host "Done. Artifacts in $buildDir" -ForegroundColor Green
