#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"

#include "board_config.h"

#define HEARTBEAT_MS 5000
#define I2C_BAUD 400000
#define SPI_BAUD 2000000

static void init_led(void) {
    gpio_init(RP2350_GEEK_LED_PIN);
    gpio_set_dir(RP2350_GEEK_LED_PIN, GPIO_OUT);
    gpio_put(RP2350_GEEK_LED_PIN, 0);
}

static void init_i2c(void) {
    i2c_init(RP2350_GEEK_I2C_PORT, I2C_BAUD);
    gpio_set_function(RP2350_GEEK_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(RP2350_GEEK_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(RP2350_GEEK_I2C_SDA_PIN);
    gpio_pull_up(RP2350_GEEK_I2C_SCL_PIN);
}

static int i2c_scan(uint8_t *first_address) {
    int found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t rx = 0;
        int ret = i2c_read_blocking(RP2350_GEEK_I2C_PORT, addr, &rx, 1, false);
        if (ret >= 0) {
            if (found == 0 && first_address) {
                *first_address = addr;
            }
            found++;
        }
    }
    return found;
}

static void init_spi(void) {
    spi_init(RP2350_GEEK_SPI_PORT, SPI_BAUD);
    gpio_set_function(RP2350_GEEK_SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RP2350_GEEK_SPI_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RP2350_GEEK_SPI_SCK_PIN, GPIO_FUNC_SPI);

    gpio_init(RP2350_GEEK_SPI_CS_PIN);
    gpio_set_dir(RP2350_GEEK_SPI_CS_PIN, GPIO_OUT);
    gpio_put(RP2350_GEEK_SPI_CS_PIN, 1);
}

static bool spi_loopback_test(void) {
    uint8_t tx[] = {0xAA, 0x55, 0xF0, 0x0F, 0x12, 0x34};
    uint8_t rx[sizeof(tx)] = {0};

    // Requires MOSI and MISO tied together externally for loopback.
    gpio_put(RP2350_GEEK_SPI_CS_PIN, 0);
    spi_write_read_blocking(RP2350_GEEK_SPI_PORT, tx, rx, sizeof(tx));
    gpio_put(RP2350_GEEK_SPI_CS_PIN, 1);

    return memcmp(tx, rx, sizeof(tx)) == 0;
}

static void init_adc(void) {
    adc_init();
    adc_gpio_init(RP2350_GEEK_ADC_PIN);
}

static uint16_t read_adc_raw(void) {
    uint input = 0;
    if (RP2350_GEEK_ADC_PIN >= 26) {
        input = RP2350_GEEK_ADC_PIN - 26;
    }
    adc_select_input(input);
    return adc_read();
}

static const char *arch_name(void) {
#if defined(__riscv)
    return "riscv";
#else
    return "arm";
#endif
}

int main(void) {
    stdio_init_all();
    sleep_ms(500);

    init_led();
    init_i2c();
    init_spi();
    init_adc();

    bi_decl(bi_program_description("RP2350-GEEK bare-metal bring-up demo"));
    bi_decl(bi_1pin_with_name(RP2350_GEEK_LED_PIN, "Onboard LED"));
    bi_decl(bi_1pin_with_name(RP2350_GEEK_I2C_SDA_PIN, "I2C SDA"));
    bi_decl(bi_1pin_with_name(RP2350_GEEK_I2C_SCL_PIN, "I2C SCL"));
    bi_decl(bi_1pin_with_name(RP2350_GEEK_SPI_MOSI_PIN, "SPI MOSI"));
    bi_decl(bi_1pin_with_name(RP2350_GEEK_SPI_MISO_PIN, "SPI MISO"));
    bi_decl(bi_1pin_with_name(RP2350_GEEK_SPI_SCK_PIN, "SPI SCK"));
    bi_decl(bi_1pin_with_name(RP2350_GEEK_SPI_CS_PIN, "SPI CS"));
    bi_decl(bi_1pin_with_name(RP2350_GEEK_ADC_PIN, "ADC test pin"));

    printf("RP2350-GEEK bare-metal demo booting (arch=%s)\n", arch_name());
    printf("USB CDC and UART logging enabled. Heartbeat is %d ms.\n", HEARTBEAT_MS);
    printf("I2C baud %d, SPI baud %d.\n", I2C_BAUD, SPI_BAUD);

    uint32_t counter = 0;
    while (true) {
        counter++;
        gpio_xor_mask(1u << RP2350_GEEK_LED_PIN);

        uint8_t first_i2c = 0;
        int i2c_devices = i2c_scan(&first_i2c);
        bool spi_ok = spi_loopback_test();
        uint16_t adc_raw = read_adc_raw();
        float adc_v = (float)adc_raw * 3.3f / 4095.0f;

        printf("[heartbeat %lu] arch=%s led=%d i2c_devices=%d first=0x%02X spi_loop=%s adc=%.2fV\n",
               (unsigned long)counter,
               arch_name(),
               gpio_get_out_level(RP2350_GEEK_LED_PIN),
               i2c_devices,
               first_i2c,
               spi_ok ? "ok" : "check wiring",
               adc_v);

        sleep_ms(HEARTBEAT_MS);
    }
}
