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
#define GPIO_BTN_PREV 4  // Previous track button
#define GPIO_BTN_PLAY 5  // Play/Pause button
#define GPIO_BTN_NEXT 15 // Next track button

/*********************************
 * SD CARD SPI PINS
 ********************************/
#define GPIO_SD_MOSI 26 // SPI MOSI (Master Out Slave In)
#define GPIO_SD_MISO 19 // SPI MISO (Master In Slave Out)
#define GPIO_SD_SCLK 18 // SPI Clock
#define GPIO_SD_CS 27   // SPI Chip Select

/*********************************
 * OLED DISPLAY I2C PINS
 ********************************/
#define GPIO_OLED_SDA 21 // I2C Data
#define GPIO_OLED_SCL 22 // I2C Clock

#endif // GPIO_CONFIG_H
