#include <stdio.h>
#include "epd_display.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG_EPD = "EPD";

spi_device_handle_t epd_spi;
uint8_t oldImage[12480];

void epd_spi_init(void) {
    esp_err_t ret;
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,  // Not needed
        .mosi_io_num = PIN_SPI_MOSI,
        .sclk_io_num = PIN_SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2 * 1000 * 1000,  // 2 MHz
        .mode = 0,  // SPI mode 0
        .spics_io_num = PIN_EPD_CS,
        .queue_size = 1,
    };

    // Initialize SPI bus
    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);

    // Attach the EPD display to the SPI bus
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &epd_spi);
    ESP_ERROR_CHECK(ret);
}

void epd_gpio_init(void) {
    gpio_set_direction(PIN_EPD_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_EPD_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_EPD_BUSY, GPIO_MODE_INPUT);
    gpio_set_direction(LCD_GND_CTRL, GPIO_MODE_OUTPUT);
}

void epd_write_reg(uint8_t command) {
    gpio_set_level(PIN_EPD_DC, 0);  // Command mode
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &command,
    };
    esp_err_t ret = spi_device_transmit(epd_spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_EPD, "SPI Transmission failed with error: %s", esp_err_to_name(ret));
    }
}

void epd_write_data8(uint8_t data) {
    gpio_set_level(PIN_EPD_DC, 1);  // Data mode
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data,
    };
    esp_err_t ret = spi_device_transmit(epd_spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_EPD, "SPI Transmission failed with error: %s", esp_err_to_name(ret));
    }
}

void epd_wait_busy(void) {
    while (gpio_get_level(PIN_EPD_BUSY) == 0) {  // UC8253 is active-low
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void epd_reset(void) {
    ESP_LOGI(TAG_EPD, "Resetting UC8253 ePaper...");

    gpio_set_level(PIN_EPD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    gpio_set_level(PIN_EPD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    epd_wait_busy();
}

void epd_update(void) {
    epd_write_reg(0x04);
    epd_wait_busy();
    epd_write_reg(0x12);
    epd_wait_busy();
}

void epd_init(void) {
    epd_reset();
    epd_wait_busy();
    epd_write_reg(0x00);
    epd_write_data8(0x1B);
}

void epd_part_init(void) {
    epd_reset();
    epd_wait_busy();

    epd_write_reg(0x00);
    epd_write_data8(0x1B);
    epd_write_reg(0xE0);
    epd_write_data8(0x02);
    epd_write_reg(0xE5);
    epd_write_data8(0x6E);
    epd_write_reg(0x50);
    epd_write_data8(0xD7);

    epd_wait_busy();
}

void epd_fast_init(void) {
    epd_reset();
    epd_wait_busy();

    epd_write_reg(0x00);
    epd_write_data8(0x1B);
    epd_write_reg(0xE0);
    epd_write_data8(0x02);
    epd_write_reg(0xE5);
    epd_write_data8(0x5F);
    epd_write_reg(0x50);
    epd_write_data8(0xD7);

    epd_wait_busy();
}

void epd_clear(void) {
    ESP_LOGI(TAG_EPD, "Clearing Display...");
    
    uint16_t width = (EPD_WIDTH % 8 == 0) ? (EPD_WIDTH / 8) : (EPD_WIDTH / 8 + 1);
    uint16_t height = EPD_HEIGHT;

    epd_write_reg(0x10);
    for (uint16_t j = 0; j < height; j++) {
        for (uint16_t i = 0; i < width; i++) {
            epd_write_data8(oldImage[i + j * width]);
        }
    }

    epd_write_reg(0x13);
    for (uint16_t j = 0; j < height; j++) {
        for (uint16_t i = 0; i < width; i++) {
            epd_write_data8(0xFF);
            oldImage[i + j * width] = 0xFF;
        }
    }

    ESP_LOGI(TAG_EPD, "Display Cleared");
}

void epd_test(void) {
    ESP_LOGI(TAG_EPD, "Displaying test pattern...");

    epd_write_reg(0x10);
    for (int i = 0; i < 12480; i++) {
        epd_write_data8(0xAA);
    }

    epd_write_reg(0x13);
    for (int i = 0; i < 12480; i++) {
        epd_write_data8(0x55);
    }

    epd_write_reg(0x20);
    epd_wait_busy();

    ESP_LOGI(TAG_EPD, "Test pattern displayed.");
}

void epd_deep_sleep(void) {
    epd_write_reg(0x02);
    epd_wait_busy();
    epd_write_reg(0x07);
    epd_write_data8(0xA5);
}

void epd_enable_power(void) {
    gpio_set_level(LCD_GND_CTRL, 1);
}

/*******************************************************************
 * Function Description: Updates the e-paper display with new image data.
 * 
 * Parameters:
 *   *image - Pointer to the image buffer to be displayed
 * 
 * Returns: None
 *******************************************************************/
void epd_display(const uint8_t *image) {
    const uint16_t width_bytes = EPD_WIDTH_BYTES;
    const uint16_t height = EPD_HEIGHT;

    // Step 1: Write OLD image buffer
    epd_write_reg(0x10);
    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width_bytes; x++) {
            uint32_t index = y * width_bytes + x;
            epd_write_data8(oldImage[index]);
        }
    }

    // Step 2: Write NEW image buffer
    epd_write_reg(0x13);
    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width_bytes; x++) {
            uint32_t index = y * width_bytes + x;
            uint8_t val = image[index];
            epd_write_data8(val);
            oldImage[index] = val; // Save for next refresh
        }
    }
}

