#include <stdint.h>
#include <stdlib.h>
#include "epd_graphics.h"
#include "epd_font.h"

epd_framebuffer_t epd_fb;

/**
 * @brief Initializes the global framebuffer for the e-paper display.
 *
 * Sets up internal framebuffer metadata, including memory dimensions,
 * logical drawing dimensions (based on rotation), and default background color.
 *
 * @param buffer Pointer to an externally allocated 1-bit image buffer
 * @param width  Physical width of the display in pixels (before rotation)
 * @param height Physical height of the display in pixels (before rotation)
 * @param rotation Display rotation setting (EPD_ROTATE_0, _90, _180, _270)
 * @param background_color Background color for clearing/filling (0 = black, 1 = white)
 *
 * @return None
 */
void epd_set_buffer(uint8_t *buffer, uint16_t width, uint16_t height, epd_rotation_t rotation, uint8_t background_color)
{
    epd_fb.buffer = buffer;
    epd_fb.width_memory = width;
    epd_fb.height_memory = height;
    epd_fb.width_bytes = (width + 7) / 8;
    epd_fb.rotation = rotation;
    epd_fb.background_color = background_color;

    // Logical width/height depends on rotation
    if (rotation == EPD_ROTATE_0 || rotation == EPD_ROTATE_180) {
        epd_fb.width = height;
        epd_fb.height = width;
    } else {
        epd_fb.width = width;
        epd_fb.height = height;
    }
}

/**
 * @brief Clears the entire framebuffer with a specified color.
 *
 * Fills the internal e-paper framebuffer with either black (0x00) or white (0xFF),
 * depending on the specified color value. This prepares the display for fresh drawing.
 *
 * @param color Fill color for the buffer:
 *              - 0 = black (all bits cleared)
 *              - 1 = white (all bits set)
 *
 * @return None
 */
void epd_clear_buffer(uint8_t color) {
    uint16_t x, y;
    uint32_t addr;
    uint8_t fill = color ? WHITE : BLACK;

    for (y = 0; y < epd_fb.height_memory; y++) {
        for (x = 0; x < epd_fb.width_bytes; x++) {
            addr = y * epd_fb.width_bytes + x;
            epd_fb.buffer[addr] = fill;
        }
    }
}

void epd_clear_buffer_region(int x, int y, int w, int h, uint8_t color) {
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            epd_draw_pixel(i, j, color ? WHITE : BLACK);
        }
    }
}

/**
 * @brief Draws a single pixel in the framebuffer at the specified coordinates.
 *
 * Applies display rotation and writes the pixel color to the correct position
 * in the framebuffer. Coordinates are relative to the logical (rotated) view.
 *
 * @param x     X coordinate of the pixel (origin at bottom-left by default)
 * @param y     Y coordinate of the pixel
 * @param color Pixel color:
 *              - 0 = black (bit cleared)
 *              - 1 = white (bit set)
 *
 * @return None
 */
void epd_draw_pixel(uint16_t x, uint16_t y, uint8_t color) {
    uint16_t X, Y;

    // Apply rotation
    switch (epd_fb.rotation) {
        case EPD_ROTATE_0:
            X = y;
            Y = x;
            break;
        case EPD_ROTATE_90:
            X = x;
            Y = epd_fb.height_memory - y - 1;
            break;
        case EPD_ROTATE_180:
            X = epd_fb.width_memory - y - 1;
            Y = epd_fb.height_memory - x - 1;
            break;
        case EPD_ROTATE_270:
            X = epd_fb.width_memory - x - 1;
            Y = y;
            break;
        default:
            return; // Invalid rotation
    }

    // Bounds check
    if (X >= epd_fb.width_memory || Y >= epd_fb.height_memory) return;

    // Calculate byte index in framebuffer
    uint32_t byte_index = Y * epd_fb.width_bytes + (X / 8);
    uint8_t bit_mask = 0x80 >> (X % 8); // MSB first: 0x80, 0x40, ..., 0x01

    if (color) {
        epd_fb.buffer[byte_index] |= bit_mask;  // white = set bit
    } else {
        epd_fb.buffer[byte_index] &= ~bit_mask; // black = clear bit
    }    
}

/**
 * @brief Displays a single character on the e-paper display.
 *
 * Renders a character using the appropriate font array based on the specified size.
 * Each character is drawn pixel-by-pixel from a pre-defined font bitmap. Supports multiple font sizes.
 *
 * @param x      X coordinate (top-left corner) where the character will be drawn
 * @param y      Y coordinate (top-left corner) where the character will be drawn
 * @param chr    ASCII character to be displayed
 * @param size   Font size (supports 8, 12, 16, 24, 48)
 * @param color  Pixel color:
 *               - 0 = black (bit cleared)
 *               - 1 = white (bit set)
 */
void epd_draw_char(uint16_t x, uint16_t y, uint16_t chr, uint16_t size, uint16_t color) {
    uint16_t i, m, temp, bytes_per_char, chr_offset;
    uint16_t x0, y0;
    
    x0 = x;
    y0 = y;
    
    // Calculate the number of bytes per character in the font array
    if (size == 8) 
        bytes_per_char = 6;
    else 
        bytes_per_char = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);

    // Adjust character index based on ASCII table (subtract space ' ' character)
    chr_offset = chr - ' ';

    // Select appropriate font array based on size
    for (i = 0; i < bytes_per_char; i++) {
        if (size == 8)
            temp = ascii_0806[chr_offset][i]; // Load 0806 font
        else if (size == 12)
            temp = ascii_1206[chr_offset][i]; // Load 1206 font
        else if (size == 16)
            temp = ascii_1608[chr_offset][i]; // Load 1608 font
        else if (size == 24)
            temp = ascii_2412[chr_offset][i]; // Load 2412 font
        else if (size == 48)
            temp = ascii_4824[chr_offset][i]; // Load 4824 font
        else
            return; // Unsupported font size

        // Process each bit in the byte to draw pixels
        for (m = 0; m < 8; m++) {
            if (temp & 0x01)
                epd_draw_pixel(x, y, color);
            else
                epd_draw_pixel(x, y, !color);

            temp >>= 1; // Shift to process the next bit
            y++;
        }

        x++;

        // Handle multi-line characters
        if ((size != 8) && ((x - x0) == size / 2)) {
            x = x0;
            y0 += 8;
        }
        y = y0;
    }
}

/**
 * @brief Displays a string on the e-paper display.
 *
 * Iterates through each character in the string and draws it using `epd_draw_char()`,
 * advancing the x-coordinate by half the font size after each character.
 *
 * @param x      X coordinate of the starting position
 * @param y      Y coordinate of the starting position
 * @param str    Pointer to the null-terminated string to be displayed
 * @param size   Font size of each character
 * @param color  Pixel color (0 = black, 1 = white)
 */
void epd_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t size, uint16_t color) {
    while (*str != '\0') {  // Check if the character is valid (not null)
        epd_draw_char(x, y, *str, size, color);
        str++;
        x += size / 2;  // Move to the next character position
    }
}

/**
 * @brief Draws a monochrome bitmap image into the framebuffer at the specified position.
 *
 * The bitmap should be in horizontal byte layout (row by row, left to right),
 * where each byte represents 8 horizontal pixels (MSB first). The image is
 * automatically mirrored horizontally to correct for display orientation.
 *
 * @param x0     X position (bottom-left origin) where the image will be drawn
 * @param y0     Y position (bottom-left origin) where the image will be drawn
 * @param width  Width of the image in pixels
 * @param height Height of the image in pixels
 * @param bmp    Pointer to the bitmap data (1-bit per pixel, horizontal layout)
 * @param color  Foreground color for set bits:
 *               - 0 = black
 *               - 1 = white
 *
 * @return None
 */
void epd_draw_image(uint16_t x0, uint16_t y0, uint16_t width, uint16_t height, const uint8_t *bmp, uint8_t color)
{
    uint16_t byte_index = 0;
    uint16_t y_start = y0;
    uint16_t total_bytes = width * ((height + 7) / 8);

    for (uint16_t i = 0; i < total_bytes; i++) {
        uint8_t byte = bmp[byte_index++];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((y0 - y_start) >= height) break;

            uint8_t bit_val = (byte & 0x80) ? 1 : 0;
            epd_draw_pixel(x0, y0, bit_val ? color : !color);

            y0++;
            byte <<= 1;
        }

        // Finished one column → go to next x
        if ((y0 - y_start) == height) {
            y0 = y_start;
            x0++;
        }
    }
}

/**
 * @brief Draw a binary image to the e-paper framebuffer in column-major order.
 *
 * This function decodes a 1-bit-per-pixel image where each byte contains 8 vertical pixels.
 * The image data is stored in column-major format (i.e., top-down in each column, then left to right).
 *
 * @param bin Pointer to the binary image buffer. Each column is stored vertically.
 * @param x   X coordinate (in pixels) of the top-left corner where the image will be drawn.
 * @param y   Y coordinate (in pixels) of the top-left corner where the image will be drawn.
 * @param w   Width of the image in pixels.
 * @param h   Height of the image in pixels.
 *
 * @note This function updates only the framebuffer. You must call `epd_display()` and `epd_update()`
 *       to reflect changes on the physical display.
 */
void epd_draw_bin_image(const uint8_t *bin, int x, int y, int w, int h)
{
    int bytes_per_col = (h + 7) / 8;

    for (int col = 0; col < w; col++) {
        for (int row_byte = 0; row_byte < bytes_per_col; row_byte++) {
            int index = col * bytes_per_col + row_byte;
            uint8_t byte = bin[index];

            for (int bit = 0; bit < 8; bit++) {
                int row = row_byte * 8 + bit;
                if (row >= h) break;

                bool pixel_on = (byte >> (7 - bit)) & 0x01;
                epd_draw_pixel(x + col, y + row, pixel_on ? BLACK : WHITE);
            }
        }
    }
}

/**
 * @brief Draws a circle using the midpoint circle algorithm.
 *
 * @param x0    X coordinate of the circle center
 * @param y0    Y coordinate of the circle center
 * @param r     Radius of the circle
 * @param color Pixel color (0 = black, 1 = white)
 * @param fill  If true, draws a filled circle. If false, draws only the outline.
 */
void epd_draw_circle(uint16_t x0, uint16_t y0, uint16_t r, uint8_t color, bool fill)
{
    int x = 0;
    int y = r;
    int d = 3 - (r << 1);

    if (fill) {
        while (x <= y) {
            for (int i = x; i <= y; i++) {
                epd_draw_pixel(x0 + x, y0 + i, color);
                epd_draw_pixel(x0 - x, y0 + i, color);
                epd_draw_pixel(x0 - i, y0 + x, color);
                epd_draw_pixel(x0 - i, y0 - x, color);
                epd_draw_pixel(x0 - x, y0 - i, color);
                epd_draw_pixel(x0 + x, y0 - i, color);
                epd_draw_pixel(x0 + i, y0 - x, color);
                epd_draw_pixel(x0 + i, y0 + x, color);
            }
            if (d < 0) {
                d += 4 * x + 6;
            } else {
                d += 10 + 4 * (x - y);
                y--;
            }
            x++;
        }
    } else {
        while (x <= y) {
            epd_draw_pixel(x0 + x, y0 + y, color);
            epd_draw_pixel(x0 - x, y0 + y, color);
            epd_draw_pixel(x0 - y, y0 + x, color);
            epd_draw_pixel(x0 - y, y0 - x, color);
            epd_draw_pixel(x0 - x, y0 - y, color);
            epd_draw_pixel(x0 + x, y0 - y, color);
            epd_draw_pixel(x0 + y, y0 - x, color);
            epd_draw_pixel(x0 + y, y0 + x, color);

            if (d < 0) {
                d += 4 * x + 6;
            } else {
                d += 10 + 4 * (x - y);
                y--;
            }
            x++;
        }
    }
}
