# RP2350-GEEK Hardware Notes

This board overview is distilled from the Waveshare wiki at https://www.waveshare.com/wiki/RP2350-GEEK.

## Highlights
- RP2350 MCU with dual architectures: dual ARM Cortex-M33 cores and dual Hazard3 RISC-V cores, clocking up to 150 MHz.
- 520 KB SRAM and 16 MB on-board Flash.
- 1.14" 240x135 IPS LCD (SPI), TF (microSD) slot, and USB-A host connector.
- 3-pin SWD header (CMSIS-DAP), 3-pin UART header, and 4-pin I2C header for external targets.
- USB-C for power/device, plastic case, and cables included.

## Pin / Peripheral Pointers
- **LED**: Use the SDK-defined `PICO_DEFAULT_LED_PIN` (GP25 on Pico2-compatible layouts). The bare-metal demo toggles this every 5 seconds.
- **UART header**: Default UART0 TX/RX match `PICO_DEFAULT_UART_TX_PIN`/`PICO_DEFAULT_UART_RX_PIN`. Change via `-DRP2350_GEEK_UART_*` cache entries if your wiring differs.
- **I2C header**: Wired for I2C0 (typical SDA=4, SCL=5). The bare-metal demo performs a bus scan each heartbeat.
- **SPI / TF card**: The demo uses SPI0 pins (MOSI=19, MISO=16, SCK=18, CS=17) for a loopback self-test. Tie MOSI to MISO to verify wiring. Adjust with `-DRP2350_GEEK_SPI_*` if your layout differs.
- **ADC**: Uses GPIO26 (ADC0) by default. You can point `RP2350_GEEK_ADC_PIN` at any ADC-capable pad.
- **LCD**: The board mounts an SPI LCD (ST7789-class). This repo does not ship a driver; reuse the pins from the Waveshare demo if you want to extend the bare-metal sample.
- **SWD**: 3-pin debug header supports CMSIS-DAP with OpenOCD. Useful for flashing and debugging via picoprobe.

## Building Blocks per Example
- **Bare-metal (Pico SDK)**: Exercises LED, UART/USB logging, I2C scan, SPI loopback, and ADC readout in a 5-second heartbeat.
- **Zephyr**: Heartbeat blinks/logs every 5 seconds using `led0`. Extend with Zephyr drivers (I2C/SPI/UART) as needed; the provided `rpi_pico2.overlay` binds the LED alias.

## Flashing Tips
- **UF2 drag-and-drop**: Hold BOOTSEL, plug USB-C, release BOOTSEL, then copy the generated `.uf2` to the mass-storage device.
- **OpenOCD / picoprobe**: `openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" -c "program build/<target>.elf verify reset exit"`

Refer to the Waveshare schematic for exact pinouts: https://files.waveshare.com/wiki/RP2350-GEEK/RP2350-GEEK.pdf.
