cmake_minimum_required(VERSION 3.24)

# Top-level project for RP2350-GEEK reference applications.
project(rp2350_geek C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Allow users to point at an existing pico-sdk checkout without copying pico_sdk_import.cmake
# into this repository. The SDK path may be provided as a cache entry or environment variable.
if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()

if(NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH is not set. Point it at your pico-sdk checkout or pass -DPICO_SDK_PATH=/path/to/pico-sdk.")
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Architecture selection: rp2350 (ARM cores) or rp2350-riscv (Hazard3 cores)
if(NOT DEFINED PICO_PLATFORM)
    set(PICO_PLATFORM "rp2350" CACHE STRING "Target platform for RP2350 (rp2350 or rp2350-riscv)")
endif()

# Hazard3 builds should stick with hardware spinlocks (software spinlocks are not provided in the SDK for this platform).
if(PICO_PLATFORM STREQUAL "rp2350-riscv")
    set(PICO_USE_SW_SPIN_LOCKS 0 CACHE BOOL "Use hardware spinlocks on rp2350-riscv" FORCE)
endif()

pico_sdk_init()

# Force ARMv8-M Mainline (Cortex-M33) flags only on ARM builds; let RISC-V use
# its default toolchain flags.
if(PICO_PLATFORM STREQUAL "rp2350")
    add_compile_options(-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16)
    add_link_options(-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16)
elseif(PICO_PLATFORM STREQUAL "rp2350-riscv")
    # Hazard3 core uses compressed + bitmanip instructions; ensure arch/abi for C/ASM (boot2 uses Zbs).
    add_compile_options(-march=rv32imc_zicsr_zifencei_zba_zbb_zbs_zbkb -mabi=ilp32)
    add_link_options(-march=rv32imc_zicsr_zifencei_zba_zbb_zbs_zbkb -mabi=ilp32)
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -march=rv32imc_zicsr_zifencei_zba_zbb_zbs_zbkb -mabi=ilp32")
    add_compile_definitions(PICO_USE_SW_SPIN_LOCKS=0)
endif()

add_subdirectory(examples/baremetal)
