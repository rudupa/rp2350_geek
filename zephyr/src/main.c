#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(rp2350_geek_demo, LOG_LEVEL_INF);

#define HEARTBEAT_MS 5000
#define LCD_STEP_MS 5000
#define PHASE_OFFSET_MS 2500

static const char *const lcd_pages[] = {
    "text",
    "gradient",
    "icon",
    "pulse"
};

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

static void heartbeat_task(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    uint32_t start_delay_ms = POINTER_TO_UINT(p1);
    if (start_delay_ms) {
        k_msleep(start_delay_ms);
    }

    uint32_t counter = 0;
    while (true) {
        counter++;
        LOG_INF("heartbeat %u arch=%s led=%d", counter,
#if defined(CONFIG_RISCV)
                "riscv",
#else
                "arm",
#endif
                led.port ? gpio_pin_get_dt(&led) : -1);
        k_msleep(HEARTBEAT_MS);
    }
}

static void lcd_task(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    uint32_t start_delay_ms = POINTER_TO_UINT(p1);
    if (start_delay_ms) {
        k_msleep(start_delay_ms);
    }

    size_t page = 0;
    while (true) {
        const char *page_name = lcd_pages[page % ARRAY_SIZE(lcd_pages)];
        LOG_INF("lcd page=%s", page_name);
        page++;
        k_msleep(LCD_STEP_MS);
    }
}

K_THREAD_STACK_DEFINE(hb0_stack, 1024);
K_THREAD_STACK_DEFINE(hb1_stack, 1024);
K_THREAD_STACK_DEFINE(lcd0_stack, 1024);
K_THREAD_STACK_DEFINE(lcd1_stack, 1024);
static struct k_thread hb0_thread;
static struct k_thread hb1_thread;
static struct k_thread lcd0_thread;
static struct k_thread lcd1_thread;

int main(void) {
    int ret = 0;

    if (led.port && device_is_ready(led.port)) {
        ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
        if (ret != 0) {
            LOG_ERR("Failed to init LED pin (%d)", ret);
        } else {
            /* Hold backlight/LED on steadily */
            gpio_pin_set_dt(&led, 1);
        }
    } else {
        LOG_WRN("No LED alias present; heartbeat will only log");
    }

    k_tid_t hb0 = k_thread_create(&hb0_thread, hb0_stack, K_THREAD_STACK_SIZEOF(hb0_stack),
                                  heartbeat_task, UINT_TO_POINTER(0), NULL, NULL,
                                  K_PRIO_PREEMPT(0), 0, K_NO_WAIT);
    k_thread_name_set(hb0, "hb0");

    k_tid_t lcd0 = k_thread_create(&lcd0_thread, lcd0_stack, K_THREAD_STACK_SIZEOF(lcd0_stack),
                                   lcd_task, UINT_TO_POINTER(0), NULL, NULL,
                                   K_PRIO_PREEMPT(0), 0, K_NO_WAIT);
    k_thread_name_set(lcd0, "lcd0");

    k_tid_t hb1 = k_thread_create(&hb1_thread, hb1_stack, K_THREAD_STACK_SIZEOF(hb1_stack),
                                  heartbeat_task, UINT_TO_POINTER(PHASE_OFFSET_MS), NULL, NULL,
                                  K_PRIO_PREEMPT(0), 0, K_NO_WAIT);
    k_thread_name_set(hb1, "hb1");

    k_tid_t lcd1 = k_thread_create(&lcd1_thread, lcd1_stack, K_THREAD_STACK_SIZEOF(lcd1_stack),
                                   lcd_task, UINT_TO_POINTER(PHASE_OFFSET_MS), NULL, NULL,
                                   K_PRIO_PREEMPT(0), 0, K_NO_WAIT);
    k_thread_name_set(lcd1, "lcd1");

    return 0;
}
