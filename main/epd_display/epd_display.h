#ifndef _EDP_DISPLAY_H
#define _EDP_DISPLAY_H

#include <stdint.h>

#define EPD_WIDTH  240
#define EPD_HEIGHT  416

#define PIN_SPI_MOSI  11
#define PIN_SPI_CLK   12
#define PIN_EPD_CS    45
#define PIN_EPD_DC    46
#define PIN_EPD_RST   47
#define PIN_EPD_BUSY  48

#define LCD_GND_CTRL 7  // IO7_LCD_3.3_CTL

#define EPD_WIDTH_BYTES ((EPD_WIDTH + 7) / 8)
#define EPD_BUF_SIZE    (EPD_WIDTH_BYTES * EPD_HEIGHT)

void epd_spi_init(void);
void epd_gpio_init(void);
void epd_write_reg(uint8_t command);
void epd_write_data8(uint8_t data);
void epd_wait_busy(void);
void epd_reset(void);
void epd_update(void);
void epd_init(void);
void epd_part_init(void);
void epd_fast_init(void);
void epd_clear(void);
void epd_test(void);
void epd_deep_sleep(void);
void epd_enable_power(void);
void epd_display(const uint8_t *image);

#endif // _EDP_DISPLAY_H