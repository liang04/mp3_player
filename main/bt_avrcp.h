/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __BT_AVRCP_H__
#define __BT_AVRCP_H__

#include "esp_avrc_api.h"
#include <stdint.h>

/**
 * @brief Initialize AVRCP controller
 */
void bt_avrcp_init(void);

/**
 * @brief AVRCP controller callback
 * @param event AVRCP event
 * @param param Event parameters
 */
void bt_avrcp_ct_callback(esp_avrc_ct_cb_event_t event,
                          esp_avrc_ct_cb_param_t *param);

/**
 * @brief Handle AVRCP controller events
 * @param event Event ID
 * @param p_param Event parameters
 */
void bt_avrcp_hdl_evt(uint16_t event, void *p_param);

#endif /* __BT_AVRCP_H__ */
