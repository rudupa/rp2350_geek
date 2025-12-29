#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(rp2350_geek_demo, LOG_LEVEL_INF);

#define HEARTBEAT_MS 5000

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

static void blink_once(uint32_t counter) {
    if (!device_is_ready(led.port)) {
        LOG_ERR("LED GPIO controller not ready");
        return;
    }

    gpio_pin_toggle_dt(&led);

    LOG_INF("heartbeat %u arch=%s led=%d",
            counter,
#if defined(CONFIG_RISCV)
            "riscv",
#else
            "arm",
#endif
            gpio_pin_get_dt(&led));
}

int main(void) {
    int ret = 0;

    if (led.port && device_is_ready(led.port)) {
        ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
        if (ret != 0) {
            LOG_ERR("Failed to init LED pin (%d)", ret);
        }
    } else {
        LOG_WRN("No LED alias present; heartbeat will only log");
    }

    uint32_t counter = 0;
    while (true) {
        counter++;
        blink_once(counter);
        k_msleep(HEARTBEAT_MS);
    }
}
