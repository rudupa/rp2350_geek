# Open Source Contribution TODO

Tracked gaps to upstream/fix in Zephyr and related tooling for RP2350-GEEK / rpi_pico2.

## Zephyr board support gaps
- [ ] Enable SMP for rp2350/rpi_pico2: DTS/SoC updates, arch barriers/spinlocks, two-core sanity demo. _(Complexity: high; Impact: high — unlocks dual-core use.)_
- [ ] Add USB CDC ACM on this board: clock/pin mux, power, Kconfig overlay, basic echo validation. _(Complexity: medium; Impact: high — restores USB console.)_
- [ ] Wire LCD via SPI controller (no bit-bang): SPI1 + CS/DC/RESET/BL pins, ST7789 offsets/MADCTL, sample app overlay. _(Complexity: medium; Impact: medium — proper display support.)_
- [ ] Improve led0/backlight aliases and UART0 pin documentation in board DTS/overlay. _(Complexity: low; Impact: medium — reduces user confusion.)_

## Flashing and tooling
- [ ] Make serial BOOTSEL helper more robust or upstream a west runner tweak; document failure modes. _(Complexity: medium; Impact: medium — smoother flashing.)_

## Docs and validation artifacts
- [ ] Add minimal repro overlays/configs plus logs/screenshots for each new feature (SMP, USB CDC, LCD, flashing). _(Complexity: low; Impact: medium — eases review and adoption.)_
