/*
 * Simple SSD1306 OLED Driver for ESP32
 * Supports 128x32 displays via I2C
 */

#ifndef __SSD1306_DRV_H__
#define __SSD1306_DRV_H__

#include "driver/i2c.h"
#include <stdbool.h>
#include <stdint.h>

// SSD1306 I2C Address
#define SSD1306_I2C_ADDR 0x3C

// Display dimensions
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 32

// SSD1306 Commands
#define SSD1306_CMD_DISPLAY_OFF 0xAE
#define SSD1306_CMD_DISPLAY_ON 0xAF
#define SSD1306_CMD_SET_CONTRAST 0x81
#define SSD1306_CMD_SET_MULTIPLEX 0xA8
#define SSD1306_CMD_SET_DISPLAY_OFFSET 0xD3
#define SSD1306_CMD_SET_START_LINE 0x40
#define SSD1306_CMD_SET_SEGMENT_REMAP 0xA1
#define SSD1306_CMD_SET_COM_SCAN_DEC 0xC8
#define SSD1306_CMD_SET_COM_PINS 0xDA
#define SSD1306_CMD_SET_PRECHARGE 0xD9
#define SSD1306_CMD_SET_VCOM_DETECT 0xDB
#define SSD1306_CMD_SET_DISPLAY_CLOCK_DIV 0xD5
#define SSD1306_CMD_SET_CHARGE_PUMP 0x8D
#define SSD1306_CMD_ENTIRE_DISPLAY_ON 0xA4
#define SSD1306_CMD_NORMAL_DISPLAY 0xA6
#define SSD1306_CMD_SET_MEMORY_MODE 0x20
#define SSD1306_CMD_SET_COLUMN_ADDR 0x21
#define SSD1306_CMD_SET_PAGE_ADDR 0x22

/**
 * @brief Initialize SSD1306 OLED display
 * @param i2c_num I2C port number
 * @return ESP_OK on success
 */
esp_err_t ssd1306_init(i2c_port_t i2c_num);

/**
 * @brief Clear the entire display
 * @param i2c_num I2C port number
 */
void ssd1306_clear(i2c_port_t i2c_num);

/**
 * @brief Display a text string on the screen
 * @param i2c_num I2C port number
 * @param page Page number (0-3 for 32-pixel height)
 * @param text Text string to display
 */
void ssd1306_text(i2c_port_t i2c_num, uint8_t page, const char *text);

/**
 * @brief Set display contrast
 * @param i2c_num I2C port number
 * @param contrast Contrast value (0-255)
 */
void ssd1306_contrast(i2c_port_t i2c_num, uint8_t contrast);

#endif /* __SSD1306_DRV_H__ */
