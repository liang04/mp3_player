/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef GPIO_CONFIG_H
#define GPIO_CONFIG_H

/**
 * @file gpio_config.h
 * @brief Centralized GPIO pin configuration for the A2DP source project
 *
 * This file contains all GPIO pin definitions used throughout the project.
 * Modify these definitions to adapt to different hardware configurations.
 */

/*********************************
 * BUTTON CONTROL PINS
 ********************************/
#define GPIO_BTN_PREV 26     // Previous track button (KEY1)
#define GPIO_BTN_PLAY 27     // Play/Pause button (KEY2)
#define GPIO_BTN_NEXT 22     // Next track button (KEY3)
#define GPIO_BTN_VOL_UP 23   // Volume Up button
#define GPIO_BTN_VOL_DOWN 33 // Volume Down button

/*********************************
 * SD CARD SPI PINS
 ********************************/
#define GPIO_SD_MOSI 19 // SPI MOSI (Master Out Slave In)
#define GPIO_SD_MISO 5  // SPI MISO (Master In Slave Out)
#define GPIO_SD_SCLK 18 // SPI Clock
#define GPIO_SD_CS 21   // SPI Chip Select

/*********************************
 * OLED DISPLAY I2C PINS
 ********************************/
#define GPIO_OLED_SDA 16 // I2C Data (Changed to 16)
#define GPIO_OLED_SCL 17 // I2C Clock (Changed to 17)

#endif // GPIO_CONFIG_H
