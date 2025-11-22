/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "bt_avrcp.h"
#include "audio_player.h"
#include "bt_app_core.h"
#include "common.h"
#include "esp_log.h"
#include <inttypes.h>
#include <stdlib.h>

/*********************************
 * CONSTANTS
 ********************************/
#define INITIAL_VOLUME 20 // Default initial volume (about 16%)

/*********************************
 * MODULE VARIABLES
 ********************************/
esp_avrc_rn_evt_cap_mask_t s_avrc_peer_rn_cap;
bool s_volume_init_done = false;

/*********************************
 * STATIC FUNCTIONS
 ********************************/
static void bt_av_volume_changed(void) {
  if (esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_TEST,
                                         &s_avrc_peer_rn_cap,
                                         ESP_AVRC_RN_VOLUME_CHANGE)) {
    esp_avrc_ct_send_register_notification_cmd(APP_RC_CT_TL_RN_VOLUME_CHANGE,
                                               ESP_AVRC_RN_VOLUME_CHANGE, 0);
  }
}

static void bt_av_notify_evt_handler(uint8_t event_id,
                                     esp_avrc_rn_param_t *event_parameter) {
  switch (event_id) {
  /* when volume changed locally on target, this event comes */
  case ESP_AVRC_RN_VOLUME_CHANGE: {
    audio_player_set_volume(event_parameter->volume);
    ESP_LOGI(BT_RC_CT_TAG, "Volume changed on remote device: %d",
             event_parameter->volume);
    // Register for next volume change notification
    bt_av_volume_changed();
    break;
  }
  /* other */
  default:
    break;
  }
}

/*********************************
 * PUBLIC FUNCTIONS
 ********************************/
void bt_avrcp_ct_callback(esp_avrc_ct_cb_event_t event,
                          esp_avrc_ct_cb_param_t *param) {
  switch (event) {
  case ESP_AVRC_CT_CONNECTION_STATE_EVT:
  case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
  case ESP_AVRC_CT_METADATA_RSP_EVT:
  case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
  case ESP_AVRC_CT_REMOTE_FEATURES_EVT:
  case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT:
  case ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT: {
    bt_app_work_dispatch(bt_avrcp_hdl_evt, event, param,
                         sizeof(esp_avrc_ct_cb_param_t), NULL);
    break;
  }
  default: {
    ESP_LOGE(BT_RC_CT_TAG, "Invalid AVRC event: %d", event);
    break;
  }
  }
}

void bt_avrcp_hdl_evt(uint16_t event, void *p_param) {
  ESP_LOGD(BT_RC_CT_TAG, "%s evt %d", __func__, event);
  esp_avrc_ct_cb_param_t *rc = (esp_avrc_ct_cb_param_t *)(p_param);

  switch (event) {
  /* when connection state changed, this event comes */
  case ESP_AVRC_CT_CONNECTION_STATE_EVT: {
    uint8_t *bda = rc->conn_stat.remote_bda;
    ESP_LOGI(BT_RC_CT_TAG,
             "AVRC conn_state event: state %d, [%02x:%02x:%02x:%02x:%02x:%02x]",
             rc->conn_stat.connected, bda[0], bda[1], bda[2], bda[3], bda[4],
             bda[5]);

    if (rc->conn_stat.connected) {
      esp_avrc_ct_send_get_rn_capabilities_cmd(APP_RC_CT_TL_GET_CAPS);
    } else {
      s_avrc_peer_rn_cap.bits = 0;
      s_volume_init_done = false; // Reset flag for next connection
    }
    break;
  }
  /* when passthrough responded, this event comes */
  case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT: {
    ESP_LOGI(
        BT_RC_CT_TAG,
        "AVRC passthrough response: key_code 0x%x, key_state %d, rsp_code %d",
        rc->psth_rsp.key_code, rc->psth_rsp.key_state, rc->psth_rsp.rsp_code);
    break;
  }
  /* when metadata responded, this event comes */
  case ESP_AVRC_CT_METADATA_RSP_EVT: {
    ESP_LOGI(BT_RC_CT_TAG, "AVRC metadata response: attribute id 0x%x, %s",
             rc->meta_rsp.attr_id, rc->meta_rsp.attr_text);
    free(rc->meta_rsp.attr_text);
    break;
  }
  /* when notification changed, this event comes */
  case ESP_AVRC_CT_CHANGE_NOTIFY_EVT: {
    ESP_LOGI(BT_RC_CT_TAG, "AVRC event notification: %d",
             rc->change_ntf.event_id);
    bt_av_notify_evt_handler(rc->change_ntf.event_id,
                             &rc->change_ntf.event_parameter);
    break;
  }
  /* when indicate feature of remote device, this event comes */
  case ESP_AVRC_CT_REMOTE_FEATURES_EVT: {
    ESP_LOGI(BT_RC_CT_TAG, "AVRC remote features %" PRIx32 ", TG features %x",
             rc->rmt_feats.feat_mask, rc->rmt_feats.tg_feat_flag);
    break;
  }
  /* when get supported notification events capability of peer device, this
   * event comes */
  case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT: {
    ESP_LOGI(BT_RC_CT_TAG, "remote rn_cap: count %d, bitmask 0x%x",
             rc->get_rn_caps_rsp.cap_count, rc->get_rn_caps_rsp.evt_set.bits);
    s_avrc_peer_rn_cap.bits = rc->get_rn_caps_rsp.evt_set.bits;

    // Set initial volume when AVRCP connection is established
    if (!s_volume_init_done) {
      audio_player_set_volume(INITIAL_VOLUME);
      ESP_LOGI(BT_RC_CT_TAG, "Setting initial volume to %d", INITIAL_VOLUME);
      esp_avrc_ct_send_set_absolute_volume_cmd(APP_RC_CT_TL_RN_VOLUME_CHANGE,
                                               INITIAL_VOLUME);
      s_volume_init_done = true;
    }

    bt_av_volume_changed();
    break;
  }
  /* when set absolute volume responded, this event comes */
  case ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT: {
    ESP_LOGI(BT_RC_CT_TAG, "Set absolute volume response: volume %d",
             rc->set_volume_rsp.volume);
    break;
  }
  /* other */
  default: {
    ESP_LOGE(BT_RC_CT_TAG, "%s unhandled event: %d", __func__, event);
    break;
  }
  }
}

void bt_avrcp_init(void) {
  esp_avrc_ct_init();
  esp_avrc_ct_register_callback(bt_avrcp_ct_callback);

  esp_avrc_rn_evt_cap_mask_t evt_set = {0};
  esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set,
                                     ESP_AVRC_RN_VOLUME_CHANGE);
  ESP_ERROR_CHECK(esp_avrc_tg_set_rn_evt_cap(&evt_set));
}
