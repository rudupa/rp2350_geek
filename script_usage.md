# Script Usage

Each script expects `scripts/config.ps1` to be configured (toolchain paths, target names, etc.). Paths are workspace-relative.

## scripts/build.ps1
- **What it does**: Configures CMake and builds the configured target (default `rp2350_geek_baremetal`) using the selected platform/toolchain; optionally cleans first.
- **Key args**: `-Clean` removes the build dir before configure+build. `-Platform rp2350|rp2350-riscv` selects ARM (default) vs RISC-V Hazard3 (requires a RISC-V GCC in `config.RISCV_TOOLCHAIN_DIR` or on `PATH`).
- **Example**: `pwsh -NoProfile -File scripts/build.ps1` or `pwsh -NoProfile -File scripts/build.ps1 -Clean -Platform rp2350-riscv`.
- **Rationale**: One-step configure+build that ensures the correct toolchain, generator, and PICO settings are applied without retyping long CMake flags.

## scripts/clean.ps1
- **What it does**: Deletes the configured build directory (`$config.BUILD_DIR`).
- **Key args**: None.
- **Example**: `pwsh -NoProfile -File scripts/clean.ps1`.
- **Rationale**: Quick reset of the build tree when switching platforms/boards or after a broken cache.

## scripts/flash.ps1
- **What it does**: Flashes a UF2 either via mass-storage (BOOTSEL drive) or picotool USB ROM.
- **Key args**:
  - `-Method mass|picotool` (default `mass`).
  - `-VolumeLabel` (mass-storage label, default `RP2350`).
  - `-Uf2Path` to override the default UF2.
  - `-PicotoolPath` to override the picotool binary when using `picotool` method.
- **Examples**:
  - Mass storage: `pwsh -NoProfile -File scripts/flash.ps1 -Method mass` (put board in BOOTSEL first).
  - Picotool: `pwsh -NoProfile -File scripts/flash.ps1 -Method picotool` (board must already be in BOOTSEL/ROM).
- **Rationale**: Covers the two common flashing paths: drag-drop BOOTSEL (simple, no dependencies) and picotool (works without mass storage, useful for automation).

## scripts/flash_via_serial_bootsel.ps1
- **What it does**: Sends `BOOTSEL` over the board's CDC/UART to trigger `reset_usb_boot` in firmware, waits for the ROM USB device, then loads the UF2 via picotool and reboots.
- **Key args**:
  - `-ComPort` (required) and optional `-Baud` (default 115200).
  - `-Uf2Path` to override the UF2.
  - `-PicotoolPath` to point to a USB-enabled picotool (defaults to `build/picotool-usb-vs/Release/picotool.exe` then `build/baremetal/_deps/picotool/picotool.exe`).
  - `-TimeoutSec` to adjust ROM detection wait.
- **Example**: `pwsh -NoProfile -File scripts/flash_via_serial_bootsel.ps1 -ComPort COM17 -Baud 9600`.
- **Rationale**: Enables fully hands-free flashing without unplugging: the running firmware listens for the `BOOTSEL` command, enters ROM, and picotool programs the device. Falls back to forcing BOOTSEL via `picotool reboot -f -u` if the initial trigger is missed.

## scripts/build_and_flash.ps1
- **What it does**: Runs `build.ps1` (optionally clean) and then flashes via `flash_via_serial_bootsel.ps1`.
- **Key args**: `-Clean` (passes through to build), `-Platform rp2350|rp2350-riscv`, `-ComPort` (default `COM17`), `-Baud` (default `9600`), optional `-PicotoolPath` and `-Uf2Path` to override defaults.
- **Example**: `pwsh -NoProfile -File scripts/build_and_flash.ps1 -Platform rp2350-riscv -ComPort COM17 -Baud 9600 -PicotoolPath build/picotool-usb-vs/Release/picotool.exe`.
- **Rationale**: One-step build-and-flash loop for the common serial BOOTSEL + picotool workflow.
