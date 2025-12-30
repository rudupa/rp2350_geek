# User-specific paths and defaults for local builds.
$config = @{
    PICO_SDK_PATH      = "C:/raspberrypi/pico-sdk"
    TOOLCHAIN_DIR      = "C:/ArmGNU/13.2/arm-gnu-toolchain-13.2.Rel1-mingw-w64-i686-arm-none-eabi"
    PYTHON_EXECUTABLE  = "C:/Users/rites/anaconda3/python.exe"
    CMAKE_GENERATOR    = "Ninja"
    CMAKE_SYSTEM_NAME  = "Generic"
    CMAKE_BUILD_TYPE   = "Release"
    PICO_PLATFORM      = "rp2350"
    PICO_BOARD         = "pico2"
    TARGET             = "rp2350_geek_baremetal"
    BUILD_DIR          = "build/baremetal"
    VCVARS_BAT         = "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat"
    PICOTOOL_PATH      = "build/baremetal/_deps/picotool/picotool.exe" # USB-enabled build copied here
}
