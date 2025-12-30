#pragma once

// Pin assignments default to Pico2-compatible locations and can be overridden
// via -D definitions on the CMake configure line if your wiring differs.
#ifndef RP2350_GEEK_LED_PIN
#define RP2350_GEEK_LED_PIN PICO_DEFAULT_LED_PIN
#endif

#ifndef RP2350_GEEK_UART_TX_PIN
#define RP2350_GEEK_UART_TX_PIN PICO_DEFAULT_UART_TX_PIN
#endif

#ifndef RP2350_GEEK_UART_RX_PIN
#define RP2350_GEEK_UART_RX_PIN PICO_DEFAULT_UART_RX_PIN
#endif

#ifndef RP2350_GEEK_I2C_PORT
#define RP2350_GEEK_I2C_PORT i2c0
#endif

#ifndef RP2350_GEEK_I2C_SDA_PIN
#define RP2350_GEEK_I2C_SDA_PIN 4
#endif

#ifndef RP2350_GEEK_I2C_SCL_PIN
#define RP2350_GEEK_I2C_SCL_PIN 5
#endif

#ifndef RP2350_GEEK_SPI_PORT
#define RP2350_GEEK_SPI_PORT spi0
#endif

#ifndef RP2350_GEEK_SPI_MOSI_PIN
#define RP2350_GEEK_SPI_MOSI_PIN 19
#endif

#ifndef RP2350_GEEK_SPI_MISO_PIN
#define RP2350_GEEK_SPI_MISO_PIN 16
#endif

#ifndef RP2350_GEEK_SPI_SCK_PIN
#define RP2350_GEEK_SPI_SCK_PIN 18
#endif

#ifndef RP2350_GEEK_SPI_CS_PIN
#define RP2350_GEEK_SPI_CS_PIN 17
#endif

#ifndef RP2350_GEEK_ADC_PIN
#define RP2350_GEEK_ADC_PIN 26
#endif

// On-board 1.14" ST7789-class LCD (SPI1 on the RP2350-GEEK reference wiring).
// Override these if your wiring differs.
#ifndef RP2350_GEEK_LCD_SPI_PORT
#define RP2350_GEEK_LCD_SPI_PORT spi1
#endif

#ifndef RP2350_GEEK_LCD_SPI_SCK_PIN
#define RP2350_GEEK_LCD_SPI_SCK_PIN 10
#endif

#ifndef RP2350_GEEK_LCD_SPI_MOSI_PIN
#define RP2350_GEEK_LCD_SPI_MOSI_PIN 11
#endif

#ifndef RP2350_GEEK_LCD_SPI_CS_PIN
#define RP2350_GEEK_LCD_SPI_CS_PIN 9
#endif

#ifndef RP2350_GEEK_LCD_DC_PIN
#define RP2350_GEEK_LCD_DC_PIN 8
#endif

#ifndef RP2350_GEEK_LCD_RST_PIN
#define RP2350_GEEK_LCD_RST_PIN 12
#endif

#ifndef RP2350_GEEK_LCD_BL_PIN
#define RP2350_GEEK_LCD_BL_PIN 13
#endif
