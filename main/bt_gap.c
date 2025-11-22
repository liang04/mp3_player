/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "bt_gap.h"
#include "common.h"
#include "esp_a2dp_api.h"
#include "esp_bt_device.h"
#include "esp_log.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/*********************************
 * MODULE VARIABLES
 ********************************/
esp_bd_addr_t s_peer_bda = {0};
uint8_t s_peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];

/*********************************
 * STATIC VARIABLES
 ********************************/
static const char remote_device_name[] = CONFIG_EXAMPLE_PEER_DEVICE_NAME;

/*********************************
 * UTILITY FUNCTIONS
 ********************************/
char *bda2str(esp_bd_addr_t bda, char *str, size_t size) {
  if (bda == NULL || str == NULL || size < 18) {
    return NULL;
  }

  sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x", bda[0], bda[1], bda[2], bda[3],
          bda[4], bda[5]);
  return str;
}

/*********************************
 * STATIC FUNCTIONS
 ********************************/
static bool get_name_from_eir(uint8_t *eir, uint8_t *bdname,
                              uint8_t *bdname_len) {
  uint8_t *rmt_bdname = NULL;
  uint8_t rmt_bdname_len = 0;

  if (!eir) {
    return false;
  }

  /* get complete or short local name from eir data */
  rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME,
                                           &rmt_bdname_len);
  if (!rmt_bdname) {
    rmt_bdname = esp_bt_gap_resolve_eir_data(
        eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
  }

  if (rmt_bdname) {
    if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN) {
      rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;
    }

    if (bdname) {
      memcpy(bdname, rmt_bdname, rmt_bdname_len);
      bdname[rmt_bdname_len] = '\0';
    }
    if (bdname_len) {
      *bdname_len = rmt_bdname_len;
    }
    return true;
  }

  return false;
}

static void filter_inquiry_scan_result(esp_bt_gap_cb_param_t *param) {
  char bda_str[18];
  uint32_t cod = 0;    /* class of device */
  int32_t rssi = -129; /* invalid value */
  uint8_t *eir = NULL;
  esp_bt_gap_dev_prop_t *p;

  /* handle the discovery results */
  ESP_LOGI(BT_AV_TAG, "Scanned device: %s",
           bda2str(param->disc_res.bda, bda_str, 18));
  for (int i = 0; i < param->disc_res.num_prop; i++) {
    p = param->disc_res.prop + i;
    switch (p->type) {
    case ESP_BT_GAP_DEV_PROP_COD:
      cod = *(uint32_t *)(p->val);
      ESP_LOGI(BT_AV_TAG, "--Class of Device: 0x%" PRIx32, cod);
      break;
    case ESP_BT_GAP_DEV_PROP_RSSI:
      rssi = *(int8_t *)(p->val);
      ESP_LOGI(BT_AV_TAG, "--RSSI: %" PRId32, rssi);
      break;
    case ESP_BT_GAP_DEV_PROP_EIR:
      eir = (uint8_t *)(p->val);
      break;
    case ESP_BT_GAP_DEV_PROP_BDNAME:
    default:
      break;
    }
  }

  /* search for device with MAJOR service class as "rendering" in COD */
  if (!esp_bt_gap_is_valid_cod(cod) ||
      !(esp_bt_gap_get_cod_srvc(cod) & ESP_BT_COD_SRVC_RENDERING)) {
    return;
  }

  /* search for target device in its Extended Inqury Response */
  if (eir) {
    get_name_from_eir(eir, s_peer_bdname, NULL);
    if (strcmp((char *)s_peer_bdname, remote_device_name) == 0) {
      ESP_LOGI(BT_AV_TAG, "Found a target device, address %s, name %s", bda_str,
               s_peer_bdname);
      s_a2d_state = APP_AV_STATE_DISCOVERED;
      memcpy(s_peer_bda, param->disc_res.bda, ESP_BD_ADDR_LEN);
      ESP_LOGI(BT_AV_TAG, "Cancel device discovery ...");
      esp_bt_gap_cancel_discovery();
    }
  }
}

/*********************************
 * PUBLIC FUNCTIONS
 ********************************/
void bt_gap_callback(esp_bt_gap_cb_event_t event,
                     esp_bt_gap_cb_param_t *param) {
  switch (event) {
  /* when device discovered a result, this event comes */
  case ESP_BT_GAP_DISC_RES_EVT: {
    if (s_a2d_state == APP_AV_STATE_DISCOVERING) {
      filter_inquiry_scan_result(param);
    }
    break;
  }
  /* when discovery state changed, this event comes */
  case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
    if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
      if (s_a2d_state == APP_AV_STATE_DISCOVERED) {
        s_a2d_state = APP_AV_STATE_CONNECTING;
        ESP_LOGI(BT_AV_TAG, "Device discovery stopped.");
        ESP_LOGI(BT_AV_TAG, "a2dp connecting to peer: %s", s_peer_bdname);
        /* connect source to peer device specified by Bluetooth Device Address
         */
        esp_a2d_source_connect(s_peer_bda);
      } else {
        /* not discovered, continue to discover */
        ESP_LOGI(BT_AV_TAG, "Device discovery failed, continue to discover...");
        esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
      }
    } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
      ESP_LOGI(BT_AV_TAG, "Discovery started.");
    }
    break;
  }
  /* when authentication completed, this event comes */
  case ESP_BT_GAP_AUTH_CMPL_EVT: {
    if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
      ESP_LOGI(BT_AV_TAG, "authentication success: %s",
               param->auth_cmpl.device_name);
      ESP_LOG_BUFFER_HEX(BT_AV_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
    } else {
      ESP_LOGE(BT_AV_TAG, "authentication failed, status: %d",
               param->auth_cmpl.stat);
    }
    break;
  }
  /* when Legacy Pairing pin code requested, this event comes */
  case ESP_BT_GAP_PIN_REQ_EVT: {
    ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit: %d",
             param->pin_req.min_16_digit);
    if (param->pin_req.min_16_digit) {
      ESP_LOGI(BT_AV_TAG, "Input pin code: 0000 0000 0000 0000");
      esp_bt_pin_code_t pin_code = {0};
      esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
    } else {
      ESP_LOGI(BT_AV_TAG, "Input pin code: 1234");
      esp_bt_pin_code_t pin_code;
      pin_code[0] = '1';
      pin_code[1] = '2';
      pin_code[2] = '3';
      pin_code[3] = '4';
      esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
    }
    break;
  }

#if (CONFIG_EXAMPLE_SSP_ENABLED == true)
  /* when Security Simple Pairing user confirmation requested, this event comes
   */
  case ESP_BT_GAP_CFM_REQ_EVT:
    ESP_LOGI(
        BT_AV_TAG,
        "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %06" PRIu32,
        param->cfm_req.num_val);
    esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
    break;
  /* when Security Simple Pairing passkey notified, this event comes */
  case ESP_BT_GAP_KEY_NOTIF_EVT:
    ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey: %06" PRIu32,
             param->key_notif.passkey);
    break;
  /* when Security Simple Pairing passkey requested, this event comes */
  case ESP_BT_GAP_KEY_REQ_EVT:
    ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
    break;
#endif

  /* when GAP mode changed, this event comes */
  case ESP_BT_GAP_MODE_CHG_EVT:
    ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode: %d",
             param->mode_chg.mode);
    break;
  case ESP_BT_GAP_GET_DEV_NAME_CMPL_EVT:
    if (param->get_dev_name_cmpl.status == ESP_BT_STATUS_SUCCESS) {
      ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_GET_DEV_NAME_CMPL_EVT device name: %s",
               param->get_dev_name_cmpl.name);
    } else {
      ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_GET_DEV_NAME_CMPL_EVT failed, state: %d",
               param->get_dev_name_cmpl.status);
    }
    break;
  /* other */
  default: {
    ESP_LOGI(BT_AV_TAG, "event: %d", event);
    break;
  }
  }

  return;
}

void bt_gap_register(void) { esp_bt_gap_register_callback(bt_gap_callback); }

void bt_gap_start_discovery(void) {
  ESP_LOGI(BT_AV_TAG, "Starting device discovery...");
  s_a2d_state = APP_AV_STATE_DISCOVERING;
  esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
}
