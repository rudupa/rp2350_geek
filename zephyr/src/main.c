#include <stdbool.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(rp2350_geek_demo, LOG_LEVEL_INF);

#define HEARTBEAT_MS 5000
#define LCD_STEP_MS 5000
#define PHASE_OFFSET_MS 2500

/* ST7789 1.14" LCD dimensions and offsets (controller is 240x240). */
#define LCD_WIDTH 240
#define LCD_HEIGHT 135
#define LCD_X_OFFSET 40
#define LCD_Y_OFFSET 52

#ifndef LCD_ROTATE_180
#define LCD_ROTATE_180 1
#endif

#define LCD_MADCTL_BASE 0xA0 /* portrait, RGB */
#if LCD_ROTATE_180
#define LCD_MADCTL (LCD_MADCTL_BASE ^ 0xC0) /* toggle MX/MY */
#else
#define LCD_MADCTL LCD_MADCTL_BASE
#endif

/* GPIO pins for the on-board LCD (SPI1 wiring). */
#define LCD_PIN_SCK 10
#define LCD_PIN_MOSI 11
#define LCD_PIN_CS 9
#define LCD_PIN_DC 8
#define LCD_PIN_RST 12
#define LCD_PIN_BL 13

static const struct device *const gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

static uint16_t lcd_fb[LCD_WIDTH * LCD_HEIGHT];

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
}

static inline void lcd_gpio_set(uint32_t pin, int value) {
    gpio_pin_set(gpio_dev, pin, value);
}

static void lcd_bb_write_byte(uint8_t byte) {
    for (int i = 7; i >= 0; --i) {
        lcd_gpio_set(LCD_PIN_MOSI, (byte >> i) & 0x01u);
        lcd_gpio_set(LCD_PIN_SCK, 1);
        lcd_gpio_set(LCD_PIN_SCK, 0);
    }
}

static void lcd_write_bytes(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        lcd_bb_write_byte(data[i]);
    }
}

static inline void lcd_cs(bool level) { lcd_gpio_set(LCD_PIN_CS, level); }
static inline void lcd_dc(bool level) { lcd_gpio_set(LCD_PIN_DC, level); }

static void lcd_write_cmd(uint8_t cmd) {
    lcd_cs(0);
    lcd_dc(0);
    lcd_write_bytes(&cmd, 1);
    lcd_cs(1);
}

static void lcd_write_data(const uint8_t *data, size_t len) {
    lcd_cs(0);
    lcd_dc(1);
    lcd_write_bytes(data, len);
    lcd_cs(1);
}

static void lcd_set_addr_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t x0 = x + LCD_X_OFFSET;
    uint16_t x1 = x0 + w - 1;
    uint16_t y0 = y + LCD_Y_OFFSET;
    uint16_t y1 = y0 + h - 1;

    uint8_t caset[] = {
        (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF),
        (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)
    };
    uint8_t raset[] = {
        (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
        (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)
    };

    lcd_write_cmd(0x2A);
    lcd_write_data(caset, sizeof(caset));
    lcd_write_cmd(0x2B);
    lcd_write_data(raset, sizeof(raset));
    lcd_write_cmd(0x2C);
}

static void lcd_reset_panel(void) {
    lcd_gpio_set(LCD_PIN_RST, 0);
    k_msleep(20);
    lcd_gpio_set(LCD_PIN_RST, 1);
    k_msleep(120);
}

static void lcd_init_panel(void) {
    /* Control pins */
    gpio_pin_configure(gpio_dev, LCD_PIN_CS, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(gpio_dev, LCD_PIN_DC, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(gpio_dev, LCD_PIN_RST, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(gpio_dev, LCD_PIN_BL, GPIO_OUTPUT_LOW);

    /* Bit-bang SPI pins */
    gpio_pin_configure(gpio_dev, LCD_PIN_SCK, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_dev, LCD_PIN_MOSI, GPIO_OUTPUT_LOW);

    lcd_reset_panel();

    lcd_write_cmd(0x36); /* MADCTL */
    uint8_t madctl = LCD_MADCTL;
    lcd_write_data(&madctl, 1);

    lcd_write_cmd(0x3A); /* COLMOD */
    uint8_t colmod = 0x55; /* 16-bit */
    lcd_write_data(&colmod, 1);

    lcd_write_cmd(0x21); /* INVON */
    lcd_write_cmd(0x11); /* SLPOUT */
    k_msleep(120);

    lcd_set_addr_window(0, 0, LCD_WIDTH, LCD_HEIGHT);
    lcd_write_cmd(0x29); /* DISPON */

    lcd_gpio_set(LCD_PIN_BL, 1);
}

static void lcd_flush_framebuffer(void) {
    lcd_set_addr_window(0, 0, LCD_WIDTH, LCD_HEIGHT);
    lcd_cs(0);
    lcd_dc(1);

    const size_t total = LCD_WIDTH * LCD_HEIGHT;
    uint8_t chunk[256];
    size_t sent = 0;
    while (sent < total) {
        size_t px = MIN(total - sent, (size_t)128);
        for (size_t i = 0; i < px; i++) {
            uint16_t c = lcd_fb[sent + i];
            chunk[2 * i] = (uint8_t)(c >> 8);
            chunk[2 * i + 1] = (uint8_t)(c & 0xFF);
        }
        lcd_write_bytes(chunk, px * 2);
        sent += px;
    }

    lcd_cs(1);
}

static inline void fb_set_pixel(int x, int y, uint16_t color) {
    if ((unsigned)x < LCD_WIDTH && (unsigned)y < LCD_HEIGHT) {
        lcd_fb[y * LCD_WIDTH + x] = color;
    }
}

static void fb_clear(uint16_t color) {
    for (size_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; ++i) {
        lcd_fb[i] = color;
    }
}

static void fb_draw_rect(int x, int y, int w, int h, uint16_t color) {
    for (int iy = 0; iy < h; ++iy) {
        int yy = y + iy;
        if (yy < 0 || yy >= LCD_HEIGHT) continue;
        for (int ix = 0; ix < w; ++ix) {
            int xx = x + ix;
            if (xx < 0 || xx >= LCD_WIDTH) continue;
            fb_set_pixel(xx, yy, color);
        }
    }
}

static const uint8_t font5x7[96][5] = {
#include "font5x7.inc"
};

static void fb_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg) {
    if (c < 32 || c > 127) c = '?';
    const uint8_t *glyph = font5x7[c - 32];
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            bool on = (glyph[col] >> row) & 0x01;
            fb_set_pixel(x + col, y + row, on ? fg : bg);
        }
        fb_set_pixel(x + 5, y + row, bg);
    }
}

static void fb_draw_text(int x, int y, const char *text, uint16_t fg, uint16_t bg) {
    int cursor_x = x;
    while (*text) {
        if (*text == '\n') {
            y += 8;
            cursor_x = x;
            text++;
            continue;
        }
        fb_draw_char(cursor_x, y, *text, fg, bg);
        cursor_x += 6;
        text++;
    }
}

static void fb_draw_char_2x(int x, int y, char c, uint16_t fg, uint16_t bg) {
    if (c < 32 || c > 127) c = '?';
    const uint8_t *glyph = font5x7[c - 32];
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            bool on = (glyph[col] >> row) & 0x01;
            uint16_t color = on ? fg : bg;
            fb_draw_rect(x + col * 2, y + row * 2, 2, 2, color);
        }
        fb_draw_rect(x + 10, y + row * 2, 2, 2, bg);
    }
}

static void fb_draw_text_2x(int x, int y, const char *text, uint16_t fg, uint16_t bg) {
    int cursor_x = x;
    while (*text) {
        if (*text == '\n') {
            y += 16;
            cursor_x = x;
            text++;
            continue;
        }
        fb_draw_char_2x(cursor_x, y, *text, fg, bg);
        cursor_x += 12;
        text++;
    }
}

static void fb_draw_icon16(int x, int y, const uint8_t *pixels, const uint16_t *palette) {
    for (int iy = 0; iy < 16; ++iy) {
        for (int ix = 0; ix < 16; ++ix) {
            uint8_t idx = pixels[iy * 16 + ix];
            fb_set_pixel(x + ix, y + iy, palette[idx]);
        }
    }
}

static const uint8_t heart_icon[16 * 16] = {
#include "heart_icon.inc"
};

static const uint16_t heart_palette[4] = {
    0x5294, /* rgb565(10,10,20) */
    0xFAD7, /* rgb565(255,120,120) */
    0xF9B2, /* rgb565(255,70,70) */
    0xE984, /* rgb565(200,20,20) */
};

static const uint16_t gif_palette[3] = {
    0x0000,
    0x57BF, /* rgb565(80,220,255) */
    0xFFFF,
};

static const uint8_t gif_frames[3][12 * 12] = {
#include "gif_frames.inc"
};

static void render_text_page(void) {
    uint16_t bg = rgb565(8, 16, 32);
    fb_clear(bg);
    fb_draw_text_2x(8, 10, "RP2350-GEEK", rgb565(255, 215, 64), bg);
    fb_draw_text_2x(8, 34, "Zephyr LCD demo", rgb565(200, 240, 255), bg);
    fb_draw_text_2x(8, 58, "Heartbeat+pages", rgb565(180, 255, 200), bg);
    fb_draw_text_2x(8, 82, "UART0@115200", rgb565(180, 180, 255), bg);
}

static void render_gradient_page(void) {
    for (int y = 0; y < LCD_HEIGHT; ++y) {
        for (int x = 0; x < LCD_WIDTH; ++x) {
            uint8_t r = (uint8_t)((x * 255) / LCD_WIDTH);
            uint8_t g = (uint8_t)((y * 255) / LCD_HEIGHT);
            uint8_t b = (uint8_t)(((x + y) * 255) / (LCD_WIDTH + LCD_HEIGHT));
            lcd_fb[y * LCD_WIDTH + x] = rgb565(r, g, b);
        }
    }
    fb_draw_rect(12, 12, LCD_WIDTH - 24, LCD_HEIGHT - 24, rgb565(0, 0, 0));
    fb_draw_rect(14, 14, LCD_WIDTH - 28, LCD_HEIGHT - 28, rgb565(255, 255, 255));
    fb_draw_text_2x(20, 18, "Gradient + frame", rgb565(0, 0, 0), rgb565(255, 255, 255));
}

static void render_icon_page(void) {
    fb_clear(rgb565(12, 12, 18));
    fb_draw_text_2x(12, 12, "Icon demo", rgb565(220, 220, 255), rgb565(12, 12, 18));
    fb_draw_icon16((LCD_WIDTH - 16) / 2, (LCD_HEIGHT - 16) / 2, heart_icon, heart_palette);
}

static void render_gif_page(void) {
    fb_clear(rgb565(0, 0, 0));
    fb_draw_text_2x(8, 8, "GIF-ish pulse", rgb565(120, 220, 255), rgb565(0, 0, 0));
    int origin_x = (LCD_WIDTH - 12) / 2;
    int origin_y = (LCD_HEIGHT - 12) / 2;
    for (int f = 0; f < 3; ++f) {
        fb_draw_rect(0, 32, LCD_WIDTH, LCD_HEIGHT - 32, rgb565(0, 0, 0));
        for (int iy = 0; iy < 12; ++iy) {
            for (int ix = 0; ix < 12; ++ix) {
                uint8_t idx = gif_frames[f][iy * 12 + ix];
                fb_set_pixel(origin_x + ix, origin_y + iy, gif_palette[idx]);
            }
        }
        lcd_flush_framebuffer();
        k_msleep(160);
    }
}

static const char *lcd_page_name(int page) {
    switch (page) {
        case 0: return "text";
        case 1: return "gradient";
        case 2: return "icon";
        case 3: return "pulse";
        default: return "unknown";
    }
}

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
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    int page = 0;
    while (true) {
        LOG_INF("lcd page=%s", lcd_page_name(page));
        switch (page) {
            case 0:
                render_text_page();
                lcd_flush_framebuffer();
                break;
            case 1:
                render_gradient_page();
                lcd_flush_framebuffer();
                break;
            case 2:
                render_icon_page();
                lcd_flush_framebuffer();
                break;
            case 3:
                render_gif_page();
                break;
            default:
                page = 0;
                continue;
        }

        page = (page + 1) % 4;
        k_msleep(LCD_STEP_MS);
    }
}

K_THREAD_STACK_DEFINE(hb0_stack, 1024);
K_THREAD_STACK_DEFINE(hb1_stack, 1024);
K_THREAD_STACK_DEFINE(lcd0_stack, 2048);
static struct k_thread hb0_thread;
static struct k_thread hb1_thread;
static struct k_thread lcd0_thread;

int main(void) {
    if (!device_is_ready(gpio_dev)) {
        LOG_ERR("gpio0 not ready");
        return 0;
    }

    if (led.port && device_is_ready(led.port)) {
        int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
        if (ret != 0) {
            LOG_ERR("Failed to init LED pin (%d)", ret);
        } else {
            gpio_pin_set_dt(&led, 1);
        }
    } else {
        LOG_WRN("No LED alias present; heartbeat will only log");
    }

    lcd_init_panel();

    k_tid_t hb0 = k_thread_create(&hb0_thread, hb0_stack, K_THREAD_STACK_SIZEOF(hb0_stack),
                                  heartbeat_task, UINT_TO_POINTER(0), NULL, NULL,
                                  K_PRIO_PREEMPT(0), 0, K_NO_WAIT);
    k_thread_name_set(hb0, "hb0");

    k_tid_t hb1 = k_thread_create(&hb1_thread, hb1_stack, K_THREAD_STACK_SIZEOF(hb1_stack),
                                  heartbeat_task, UINT_TO_POINTER(PHASE_OFFSET_MS), NULL, NULL,
                                  K_PRIO_PREEMPT(0), 0, K_NO_WAIT);
    k_thread_name_set(hb1, "hb1");

    k_tid_t lcd0 = k_thread_create(&lcd0_thread, lcd0_stack, K_THREAD_STACK_SIZEOF(lcd0_stack),
                                   lcd_task, NULL, NULL, NULL,
                                   K_PRIO_PREEMPT(0), 0, K_NO_WAIT);
    k_thread_name_set(lcd0, "lcd0");

    return 0;
}
