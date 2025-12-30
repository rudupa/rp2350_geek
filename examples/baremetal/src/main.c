#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/bootrom.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"

#include "board_config.h"

#define HEARTBEAT_MS 5000
#define I2C_BAUD 400000
#define SPI_BAUD 2000000
#define LCD_SPI_BAUD 40000000
#ifndef LCD_INVERT_DISPLAY
#define LCD_INVERT_DISPLAY 1
#endif

// ST7789 1.14" LCD settings (240x135 panel on 240x240 controller window).
#define LCD_WIDTH 240
#define LCD_HEIGHT 135
#define LCD_X_OFFSET 40
#define LCD_Y_OFFSET 52

#ifndef LCD_ROTATE_180
#define LCD_ROTATE_180 1
#endif

#define LCD_MADCTL_BASE 0xA0 // portrait addressing, RGB order
#if LCD_ROTATE_180
#define LCD_MADCTL (LCD_MADCTL_BASE ^ 0xC0) // toggle MX/MY for 180 deg
#else
#define LCD_MADCTL LCD_MADCTL_BASE
#endif

typedef enum {
    LCD_PAGE_TEXT = 0,
    LCD_PAGE_GRAPHIC = 1,
    LCD_PAGE_ICON = 2,
    LCD_PAGE_GIF = 3,
    LCD_PAGE_COUNT
} lcd_page_t;

static uint16_t lcd_fb[LCD_WIDTH * LCD_HEIGHT];

static void init_led(void) {
    gpio_init(RP2350_GEEK_LED_PIN);
    gpio_set_dir(RP2350_GEEK_LED_PIN, GPIO_OUT);
    gpio_put(RP2350_GEEK_LED_PIN, 1); // hold steady so LCD backlight isn't affected by toggles on shared boards
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

static const char *lcd_page_name(lcd_page_t page) {
    switch (page) {
        case LCD_PAGE_TEXT: return "text";
        case LCD_PAGE_GRAPHIC: return "gradient";
        case LCD_PAGE_ICON: return "icon";
        case LCD_PAGE_GIF: return "gif";
        default: return "unknown";
    }
}

static void check_bootsel_command(void) {
    static char buf[8];
    static uint8_t pos = 0;
    int ch;
    while ((ch = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (ch == '\r' || ch == '\n') {
            if (pos == 7) {
                const char magic[7] = {'B','O','O','T','S','E','L'};
                bool match = true;
                for (uint8_t i = 0; i < 7; ++i) {
                    char c = (char)toupper((int)buf[i]);
                    if (c != magic[i]) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    printf("BOOTSEL command received; entering ROM USB.\n");
                    sleep_ms(50);
                    reset_usb_boot(0, 0);
                }
            }
            pos = 0;
            continue;
        }
        if (pos < sizeof(buf)) {
            buf[pos++] = (char)ch;
        } else {
            pos = 0; // overflow, reset
        }
    }
}

// --- LCD helpers ---
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

#define RGB565_CONST(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

static inline void lcd_cs(bool level) {
    gpio_put(RP2350_GEEK_LCD_SPI_CS_PIN, level);
}

static inline void lcd_dc(bool level) {
    gpio_put(RP2350_GEEK_LCD_DC_PIN, level);
}

static void lcd_write_bytes(const uint8_t *data, size_t len) {
    spi_write_blocking(RP2350_GEEK_LCD_SPI_PORT, data, len);
}

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
    gpio_put(RP2350_GEEK_LCD_RST_PIN, 0);
    sleep_ms(20);
    gpio_put(RP2350_GEEK_LCD_RST_PIN, 1);
    sleep_ms(120);
}

static void lcd_init_panel(void) {
    // SPI pins
    spi_init(RP2350_GEEK_LCD_SPI_PORT, LCD_SPI_BAUD);
    spi_set_format(RP2350_GEEK_LCD_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(RP2350_GEEK_LCD_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RP2350_GEEK_LCD_SPI_MOSI_PIN, GPIO_FUNC_SPI);

    // Control pins
    gpio_init(RP2350_GEEK_LCD_SPI_CS_PIN);
    gpio_set_dir(RP2350_GEEK_LCD_SPI_CS_PIN, GPIO_OUT);
    lcd_cs(1);

    gpio_init(RP2350_GEEK_LCD_DC_PIN);
    gpio_set_dir(RP2350_GEEK_LCD_DC_PIN, GPIO_OUT);
    lcd_dc(1);

    gpio_init(RP2350_GEEK_LCD_RST_PIN);
    gpio_set_dir(RP2350_GEEK_LCD_RST_PIN, GPIO_OUT);

    gpio_init(RP2350_GEEK_LCD_BL_PIN);
    gpio_set_dir(RP2350_GEEK_LCD_BL_PIN, GPIO_OUT);

    lcd_reset_panel();

    // Minimal ST7789 init sequence for 16-bit color.
    lcd_write_cmd(0x36); // MADCTL
    uint8_t madctl = LCD_MADCTL;
    lcd_write_data(&madctl, 1);

    lcd_write_cmd(0x3A); // COLMOD
    uint8_t colmod = 0x55; // 16-bit
    lcd_write_data(&colmod, 1);

    // Optional display inversion (1 = inverted). Override via -DLCD_INVERT_DISPLAY=0.
#if LCD_INVERT_DISPLAY
    lcd_write_cmd(0x21); // INVON
#else
    lcd_write_cmd(0x20); // INVOFF
#endif
    lcd_write_cmd(0x11); // SLPOUT
    sleep_ms(120);

    lcd_set_addr_window(0, 0, LCD_WIDTH, LCD_HEIGHT);
    lcd_write_cmd(0x29); // DISPON

    gpio_put(RP2350_GEEK_LCD_BL_PIN, 1);
}

static void lcd_flush_framebuffer(void) {
    lcd_set_addr_window(0, 0, LCD_WIDTH, LCD_HEIGHT);
    lcd_cs(0);
    lcd_dc(1);

    // Stream the framebuffer in manageable chunks to avoid large stack buffers.
    const size_t total = LCD_WIDTH * LCD_HEIGHT;
    uint8_t chunk[256]; // 128 pixels per burst
    size_t sent = 0;
    while (sent < total) {
        size_t px = total - sent;
        if (px > 128) px = 128;
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

// 5x7 ASCII font (public-domain style), columns LSB->MSB for rows.
static const uint8_t font5x7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14}, {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00}, {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00}, {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08}, {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x72,0x49,0x49,0x49,0x46}, {0x21,0x41,0x49,0x4D,0x33}, {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x31}, {0x41,0x21,0x11,0x09,0x07}, {0x36,0x49,0x49,0x49,0x36}, {0x46,0x49,0x49,0x29,0x1E}, {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00}, {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x59,0x09,0x06},
    {0x3E,0x41,0x5D,0x59,0x4E}, {0x7C,0x12,0x11,0x12,0x7C}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x00}, {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7F,0x00}, {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40}, {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20}, {0x38,0x44,0x44,0x48,0x7F}, {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7E,0x09,0x01,0x02}, {0x0C,0x52,0x52,0x52,0x3E}, {0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00}, {0x20,0x40,0x44,0x3D,0x00}, {0x7F,0x10,0x28,0x44,0x00}, {0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x18,0x04,0x78}, {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38}, {0x7C,0x14,0x14,0x14,0x08}, {0x08,0x14,0x14,0x18,0x7C}, {0x7C,0x08,0x04,0x04,0x08},
    {0x48,0x54,0x54,0x54,0x20}, {0x04,0x3F,0x44,0x40,0x20}, {0x3C,0x40,0x40,0x20,0x7C}, {0x1C,0x20,0x40,0x20,0x1C}, {0x3C,0x40,0x30,0x40,0x3C}, {0x44,0x28,0x10,0x28,0x44}, {0x0C,0x50,0x50,0x50,0x3C}, {0x44,0x64,0x54,0x4C,0x44}, {0x00,0x08,0x36,0x41,0x00}, {0x00,0x00,0x7F,0x00,0x00}, {0x00,0x41,0x36,0x08,0x00}, {0x10,0x08,0x08,0x10,0x08}, {0x78,0x46,0x41,0x46,0x78}
};

static void fb_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg) {
    if (c < 32 || c > 127) c = '?';
    const uint8_t *glyph = font5x7[c - 32];
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            bool on = (glyph[col] >> row) & 0x01;
            fb_set_pixel(x + col, y + row, on ? fg : bg);
        }
        fb_set_pixel(x + 5, y + row, bg); // 1px spacing
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
        fb_draw_rect(x + 10, y + row * 2, 2, 2, bg); // spacing column at 2x
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

static void render_text_page(void) {
    uint16_t bg = rgb565(8, 16, 32);
    fb_clear(bg);
    fb_draw_text_2x(8, 10, "RP2350-GEEK", rgb565(255, 215, 64), bg);
    fb_draw_text_2x(8, 34, "Bare-metal demo", rgb565(200, 240, 255), bg);
    fb_draw_text_2x(8, 58, "I2C/SPI/ADC+LCD", rgb565(180, 255, 200), bg);
    fb_draw_text_2x(8, 82, "Send BOOTSEL to flash", rgb565(180, 180, 255), bg);
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

static const uint8_t heart_icon[16 * 16] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,
    0,1,2,2,1,0,0,0,0,1,2,2,1,0,0,0,
    1,2,3,3,2,1,0,0,1,2,3,3,2,1,0,0,
    1,3,3,3,3,2,1,1,2,3,3,3,3,2,1,0,
    1,3,3,3,3,3,2,2,3,3,3,3,3,2,1,0,
    0,2,3,3,3,3,3,3,3,3,3,3,2,2,0,0,
    0,1,2,3,3,3,3,3,3,3,3,2,1,0,0,0,
    0,0,1,2,3,3,3,3,3,3,2,1,0,0,0,0,
    0,0,0,1,2,3,3,3,3,2,1,0,0,0,0,0,
    0,0,0,0,1,2,3,3,2,1,0,0,0,0,0,0,
    0,0,0,0,0,1,2,2,1,0,0,0,0,0,0,0,
    0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static const uint16_t heart_palette[4] = {
        RGB565_CONST(10, 10, 20),
        RGB565_CONST(255, 120, 120),
        RGB565_CONST(255, 70, 70),
        RGB565_CONST(200, 20, 20),
};

static void render_icon_page(void) {
    fb_clear(rgb565(12, 12, 18));
    fb_draw_text_2x(12, 12, "Icon demo", rgb565(220, 220, 255), rgb565(12, 12, 18));
    fb_draw_icon16((LCD_WIDTH - 16) / 2, (LCD_HEIGHT - 16) / 2, heart_icon, heart_palette);
}

static const uint16_t gif_palette[3] = {
        RGB565_CONST(0, 0, 0),
        RGB565_CONST(80, 220, 255),
        RGB565_CONST(255, 255, 255),
};

static const uint8_t gif_frames[3][12 * 12] = {
    { // frame 0: small dot
        0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,1,1,1,0,0,0,0,0,
        0,0,0,0,1,2,1,0,0,0,0,0,
        0,0,0,0,1,1,1,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,
    },
    { // frame 1: medium ring
        0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,1,1,1,1,1,1,0,0,0,
        0,0,1,2,2,2,2,2,2,1,0,0,
        0,1,2,1,1,1,1,1,1,2,1,0,
        0,1,2,1,0,0,0,0,1,2,1,0,
        0,1,2,1,0,0,0,0,1,2,1,0,
        0,1,2,1,0,0,0,0,1,2,1,0,
        0,1,2,1,1,1,1,1,1,2,1,0,
        0,0,1,2,2,2,2,2,2,1,0,0,
        0,0,0,1,1,1,1,1,1,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,
    },
    { // frame 2: large ring
        0,0,1,1,1,1,1,1,1,1,1,0,
        0,1,2,2,2,2,2,2,2,2,2,1,
        1,2,1,1,1,1,1,1,1,1,1,2,
        1,2,1,0,0,0,0,0,0,0,1,2,
        1,2,1,0,0,0,0,0,0,0,1,2,
        1,2,1,0,0,0,0,0,0,0,1,2,
        1,2,1,0,0,0,0,0,0,0,1,2,
        1,2,1,0,0,0,0,0,0,0,1,2,
        1,2,1,0,0,0,0,0,0,0,1,2,
        1,2,1,1,1,1,1,1,1,1,1,2,
        0,1,2,2,2,2,2,2,2,2,2,1,
        0,0,1,1,1,1,1,1,1,1,1,0,
    }
};

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
        sleep_ms(160);
    }
}

int main(void) {
    stdio_init_all();
    sleep_ms(500);

    init_led();
    init_i2c();
    init_spi();
    init_adc();
    lcd_init_panel();

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
    lcd_page_t page = LCD_PAGE_TEXT;
    while (true) {
        counter++;

        uint8_t first_i2c = 0;
        int i2c_devices = i2c_scan(&first_i2c);
        bool spi_ok = spi_loopback_test();
        uint16_t adc_raw = read_adc_raw();
        float adc_v = (float)adc_raw * 3.3f / 4095.0f;

         printf("[heartbeat %lu] arch=%s led=%d i2c_devices=%d first=0x%02X spi_loop=%s adc=%.2fV lcd_page=%s\n",
             (unsigned long)counter,
             arch_name(),
             gpio_get_out_level(RP2350_GEEK_LED_PIN),
             i2c_devices,
             first_i2c,
             spi_ok ? "ok" : "check wiring",
             adc_v,
             lcd_page_name(page));

        switch (page) {
            case LCD_PAGE_TEXT:
                render_text_page();
                lcd_flush_framebuffer();
                page = LCD_PAGE_GRAPHIC;
                break;
            case LCD_PAGE_GRAPHIC:
                render_gradient_page();
                lcd_flush_framebuffer();
                page = LCD_PAGE_ICON;
                break;
            case LCD_PAGE_ICON:
                render_icon_page();
                lcd_flush_framebuffer();
                page = LCD_PAGE_GIF;
                break;
            case LCD_PAGE_GIF:
                render_gif_page();
                page = LCD_PAGE_TEXT;
                break;
            default:
                page = LCD_PAGE_TEXT;
                break;
        }

        // Allow host to request BOOTSEL via USB CDC/UART by sending "BOOTSEL" + Enter.
        check_bootsel_command();

        sleep_ms(HEARTBEAT_MS);
    }
}
