#ifndef _EPD_GRAPHICS_H
#define _EPD_GRAPHICS_H

#include <stdint.h>
#include <stdbool.h>

#define WHITE 0xFF
#define BLACK 0x00

typedef enum {
    EPD_ROTATE_0   = 0,  ///< No rotation
    EPD_ROTATE_90  = 90,  ///< Rotate 90 degrees clockwise
    EPD_ROTATE_180 = 180,  ///< Rotate 180 degrees
    EPD_ROTATE_270 = 270   ///< Rotate 270 degrees clockwise
} epd_rotation_t;
typedef struct {
    uint8_t *buffer;           // Pointer to raw pixel data (1-bit per pixel)
    uint16_t width;            // Logical width (after rotation, if used)
    uint16_t height;           // Logical height
    uint16_t width_memory;     // Physical width (e.g., 240)
    uint16_t height_memory;    // Physical height (e.g., 416)
    uint16_t width_bytes;      // Bytes per row = width_memory / 8
    epd_rotation_t rotation;   // EPD_ROTATE_0, EPD_ROTATE_90, EPD_ROTATE_180, EPD_ROTATE_270
    uint8_t background_color;  // Optional: used for clear/fill
} epd_framebuffer_t;

extern epd_framebuffer_t epd_fb;

void epd_set_buffer(uint8_t *buffer, uint16_t width, uint16_t height, epd_rotation_t rotation, uint8_t background_color);
void epd_clear_buffer(uint8_t color);
void epd_clear_buffer_region(int x, int y, int w, int h, uint8_t color);
void epd_draw_pixel(uint16_t x, uint16_t y, uint8_t color);
void epd_draw_char(uint16_t x, uint16_t y, uint16_t chr, uint16_t size, uint16_t color);
void epd_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t size, uint16_t color);
void epd_draw_image(uint16_t x0, uint16_t y0, uint16_t width, uint16_t height, const uint8_t *bmp, uint8_t color);
void epd_draw_bin_image(const uint8_t *bin, int x, int y, int w, int h);
void epd_draw_circle(uint16_t x0, uint16_t y0, uint16_t r, uint8_t color, bool fill);

#endif // _EPD_GRAPHICS_H