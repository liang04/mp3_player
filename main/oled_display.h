/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __OLED_DISPLAY_H__
#define __OLED_DISPLAY_H__

#include <stdint.h>

/**
 * @brief Initialize OLED display (SSD1306 128x32)
 *
 * Initializes I2C and SSD1306 OLED display controller
 * GPIO: SDA=21, SCL=22
 */
void oled_display_init(void);

/**
 * @brief Update OLED display with current playback status
 *
 * Updates display showing:
 * - Line 1: Bluetooth status, track number, song name
 * - Line 2: Progress bar and volume level
 */
void oled_display_update(void);

/**
 * @brief Clear the entire OLED display
 */
void oled_display_clear(void);

#endif /* __OLED_DISPLAY_H__ */
