/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __BUTTON_CONTROL_H__
#define __BUTTON_CONTROL_H__

/**
 * @brief Initialize button control task
 *
 * This function creates a FreeRTOS task that monitors button GPIO pins
 * and updates playback control flags.
 */
void button_control_init(void);

#endif /* __BUTTON_CONTROL_H__ */
