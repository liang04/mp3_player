/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __BT_GAP_H__
#define __BT_GAP_H__

#include "esp_gap_bt_api.h"

/**
 * @brief Register GAP callback and configure GAP
 */
void bt_gap_register(void);

/**
 * @brief Start device discovery
 */
void bt_gap_start_discovery(void);

/**
 * @brief GAP callback function
 * @param event GAP event
 * @param param Event parameters
 */
void bt_gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

#endif /* __BT_GAP_H__ */
