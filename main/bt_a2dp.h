/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __BT_A2DP_H__
#define __BT_A2DP_H__

#include "esp_a2dp_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <stdint.h>

/**
 * @brief Initialize A2DP source
 */
void bt_a2dp_init(void);

/**
 * @brief A2DP callback function
 * @param event A2DP event
 * @param param Event parameters
 */
void bt_a2dp_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);

/**
 * @brief A2DP data callback function
 * @param data Buffer to fill with audio data
 * @param len Length of buffer
 * @return Number of bytes written
 */
int32_t bt_a2dp_data_callback(uint8_t *data, int32_t len);

/**
 * @brief A2DP state machine handler
 * @param event Event ID
 * @param param Event parameters
 */
void bt_a2dp_sm_handler(uint16_t event, void *param);

/**
 * @brief Get heart beat timer handle
 * @return Timer handle
 */
TimerHandle_t bt_a2dp_get_timer(void);

#endif /* __BT_A2DP_H__ */
