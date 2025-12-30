# rp2350_geek

CMake/Ninja reference workspace for building `.uf2` images for the Waveshare RP2350-GEEK board. It includes a Pico SDK-based bare-metal demo and a Zephyr RTOS demo, each designed to run a 5-second heartbeat and exercise common board features. Board details come from the Waveshare overview: https://www.waveshare.com/wiki/RP2350-GEEK#Overview.

## Board Highlights (from Waveshare)
- RP2350 microcontroller with dual architectures: dual ARM Cortex-M33 and dual Hazard3 RISC-V cores, up to 150 MHz
- 520 KB SRAM and 16 MB on-board flash
- 1.14" 240×135 IPS LCD (SPI), TF (microSD) slot, USB-A host connector
- 3-pin SWD (CMSIS-DAP), 3-pin UART header, 4-pin I2C header
- USB-C for power/device, plastic case and cables included

## Repo Layout
- CMakeLists.txt — root build that targets Pico SDK examples
- examples/baremetal — Pico SDK heartbeat demo (LED, USB/UART log, I2C scan, SPI loopback, ADC) plus a 1.14" ST7789 LCD showcase that rotates text, gradient, icon, and a simple animated pulse on every heartbeat
- zephyr — Zephyr heartbeat demo with LED logging
- docs/hardware.md — condensed hardware and pin notes

## Prerequisites
- CMake ≥3.24, Ninja, Git
- Pico SDK checkout; set `PICO_SDK_PATH` to it (no copy of pico_sdk_import.cmake is committed here)
- ARM GCC toolchain (e.g., 13.2) for ARM builds; RISC-V toolchain for Hazard3 builds (`PICO_TOOLCHAIN_PATH`)
- Optional: OpenOCD/CMSIS-DAP or picoprobe for SWD flashing
- Zephyr toolchain + `west` if you want the Zephyr demo

## Selecting ARM vs RISC-V
- ARM (default): configure with `-DPICO_PLATFORM=rp2350`
- RISC-V: configure with `-DPICO_PLATFORM=rp2350-riscv` and set `PICO_TOOLCHAIN_PATH` to your RISC-V toolchain (see pico-sdk RISC-V quick start)
- RISC-V specifics: Hazard3 needs `zifencei` for `fence.i` and hardware spinlocks (software spinlocks are not provided). The build scripts pass `-march=rv32imc_zicsr_zifencei_zba_zbb_zbs_zbkb -mabi=ilp32` and set `PICO_USE_SW_SPIN_LOCKS=0`; ensure your toolchain supports that arch string (xPack GCC 15.2 works).

## Build and Flash — Bare-metal (Pico SDK)
1) Configure (ARM example) using a clean build dir:
	- ARM: `cmake -S . -B build/baremetal -G Ninja -DPICO_SDK_PATH=<path-to-pico-sdk> -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2`
	- RISC-V: `cmake -S . -B build/baremetal -G Ninja -DPICO_SDK_PATH=<path-to-pico-sdk> -DPICO_PLATFORM=rp2350-riscv -DPICO_BOARD=pico2 -DCMAKE_C_FLAGS="-march=rv32imc_zicsr_zifencei_zba_zbb_zbs_zbkb -mabi=ilp32" -DCMAKE_CXX_FLAGS="-march=rv32imc_zicsr_zifencei_zba_zbb_zbs_zbkb -mabi=ilp32" -DCMAKE_ASM_FLAGS="-march=rv32imc_zicsr_zifencei_zba_zbb_zbs_zbkb -mabi=ilp32"`
	- Override pins if needed, e.g., `-DRP2350_GEEK_I2C_SDA_PIN=2 -DRP2350_GEEK_I2C_SCL_PIN=3`
2) Build: `cmake --build build/baremetal -t rp2350_geek_baremetal`
3) Output: `build/baremetal/rp2350_geek_baremetal.uf2`
4) Flash: hold BOOTSEL, plug USB-C, release, copy the `.uf2` file; or use OpenOCD/picoprobe (see docs/hardware.md)

Picotool (USB-enabled) is prebuilt at `build/baremetal/_deps/picotool/picotool.exe` (copied from `build/picotool-usb-vs/Release/picotool.exe`). The script `scripts/flash_via_serial_bootsel.ps1` will use it by default and can trigger BOOTSEL over the running firmware (send `BOOTSEL` over COM then force reboot if needed) and load the UF2 via USB ROM. Use `-ComPort <port>` and optional `-Baud`, or pass `-PicotoolPath` to override.

What it does (every 5 seconds): toggles the LED, logs over USB CDC & UART, scans I2C0 (400 kHz), runs an SPI0 loopback (MOSI↔MISO), reads an ADC channel, and drives the 1.14" LCD (default SPI1 pins) through a four-page cycle (text, gradient, 16x16 heart icon, small animated pulse). Update pin defs in `examples/baremetal/board_config.h` if your wiring differs.

LCD pin defaults (SPI1): CS=9, DC=8, RST=12, BL=13, SCK=10, MOSI=11, with ST7789-style offsets (X=52, Y=40) and 16-bit color (BGR). Override via CMake cache definitions if your wiring or panel orientation differs (e.g., `-DRP2350_GEEK_LCD_SPI_CS_PIN=...`).

## Build and Flash — Zephyr RTOS Demo
1) From the repo root, build (ARM on rpi_pico2): `west build -b rpi_pico2 zephyr`
	- If your Zephyr tree provides a RISC-V board target, use it (for example `-b rpi_pico2_riscv` if available in your fork).
	- The supplied `zephyr/boards/rpi_pico2.overlay` binds `led0` to GPIO25; adjust or remove if your board file already defines an LED alias.
2) Flash: `west flash` (or copy the generated `.uf2` from `zephyr/build/zephyr/` to the BOOTSEL drive).

What it does (every 5 seconds): blinks the LED alias `led0` and logs the heartbeat plus detected architecture (`arm` vs `riscv`). Extend with Zephyr drivers (I2C/SPI/UART/LCD) as needed.

## Hardware Feature Exercise
- LED: heartbeat blinks on both demos
- UART/USB CDC: enabled in the bare-metal demo for logging
- I2C: bare-metal bus scan on I2C0
- SPI: bare-metal loopback self-test on SPI0 (wire MOSI to MISO)
- ADC: bare-metal read of one ADC-capable pin
- LCD/TF/USB-A: not driven by default; use pins from the Waveshare docs if you extend the sample
- SWD: use picoprobe/OpenOCD for flashing/debugging

## Testing Checklist
- Bare-metal build: `cmake --build build/baremetal -t rp2350_geek_baremetal`
- Bare-metal flash: drag-drop `rp2350_geek_baremetal.uf2` or `openocd ... program build/baremetal/rp2350_geek_baremetal.elf verify reset exit`
- Bare-metal run: watch USB CDC or UART log; every 5 seconds see heartbeat, I2C count, SPI loopback result, ADC voltage
- Zephyr build: `west build -b rpi_pico2 zephyr`
- Zephyr flash: `west flash` (or copy the `.uf2`)
- Zephyr run: check console log; LED should toggle every 5 seconds

## Resources
- Waveshare RP2350-GEEK wiki: https://www.waveshare.com/wiki/RP2350-GEEK#Overview
- RP2350 datasheet: https://files.waveshare.com/wiki/common/Rp2350-datasheet.pdf
- Pico SDK docs: https://rptl.io/pico-c-sdk
- Pico VS Code extension: https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico
- OpenOCD/PicoProbe guide (CMSIS-DAP): see Waveshare wiki section “PicoProbe Tutorial”
