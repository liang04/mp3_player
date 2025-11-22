/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "audio_player.h"
#include "bt_a2dp.h"
#include "bt_app_core.h"
#include "bt_avrcp.h"
#include "bt_gap.h"
#include "button_control.h"
#include "common.h"
#include "sd_card.h"

#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>

/*********************************
 * STATIC FUNCTION DEFINITION
 ********************************/
static void bt_av_hdl_stack_evt(uint16_t event, void *p_param) {
  ESP_LOGD(BT_AV_TAG, "%s event: %d", __func__, event);

  switch (event) {
  /* when stack up worked, this event comes */
  case BT_APP_STACK_UP_EVT: {
    char *dev_name = LOCAL_DEVICE_NAME;
    esp_bt_gap_set_device_name(dev_name);

    // Initialize GAP
    bt_gap_register();

    // Initialize AVRCP
    bt_avrcp_init();

    // Initialize A2DP
    bt_a2dp_init();

    /* Avoid the state error of s_a2d_state caused by the connection initiated
     * by the peer device. */
    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    esp_bt_gap_get_device_name();

    // Start device discovery
    bt_gap_start_discovery();
    break;
  }
  /* other */
  default: {
    ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
    break;
  }
  }
}

/*********************************
 * MAIN ENTRY POINT
 ********************************/
void app_main(void) {
  char bda_str[18] = {0};

  // Initialize SD card and audio player
  sd_card_init();
  audio_player_init();

  // Initialize button control
  button_control_init();

  /* initialize NVS — it is used to store PHY calibration data */
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  /*
   * This example only uses the functions of Classical Bluetooth.
   * So release the controller memory for Bluetooth Low Energy.
   */
  ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
    ESP_LOGE(BT_AV_TAG, "%s initialize controller failed", __func__);
    return;
  }
  if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) {
    ESP_LOGE(BT_AV_TAG, "%s enable controller failed", __func__);
    return;
  }

  esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
#if (CONFIG_EXAMPLE_SSP_ENABLED == false)
  bluedroid_cfg.ssp_en = false;
#endif
  if ((ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg)) != ESP_OK) {
    ESP_LOGE(BT_AV_TAG, "%s initialize bluedroid failed: %s", __func__,
             esp_err_to_name(ret));
    return;
  }

  if (esp_bluedroid_enable() != ESP_OK) {
    ESP_LOGE(BT_AV_TAG, "%s enable bluedroid failed", __func__);
    return;
  }

#if (CONFIG_EXAMPLE_SSP_ENABLED == true)
  /* set default parameters for Secure Simple Pairing */
  esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
  esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
  esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
#endif

  /*
   * Set default parameters for Legacy Pairing
   * Use variable pin, input pin code when pairing
   */
  esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
  esp_bt_pin_code_t pin_code;
  esp_bt_gap_set_pin(pin_type, 0, pin_code);

  ESP_LOGI(
      BT_AV_TAG, "Own address:[%s]",
      bda2str((uint8_t *)esp_bt_dev_get_address(), bda_str, sizeof(bda_str)));
  bt_app_task_start_up();
  /* Bluetooth device name, connection mode and profile set up */
  bt_app_work_dispatch(bt_av_hdl_stack_evt, BT_APP_STACK_UP_EVT, NULL, 0, NULL);
}
