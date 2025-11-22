/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "bt_a2dp.h"
#include "audio_player.h"
#include "bt_app_core.h"
#include "common.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <inttypes.h>

/*********************************
 * MODULE VARIABLES
 ********************************/
int s_a2d_state = APP_AV_STATE_IDLE;
int s_media_state = APP_AV_MEDIA_STATE_IDLE;
int s_intv_cnt = 0;
int s_connecting_intv = 0;
uint32_t s_pkt_cnt = 0;

/*********************************
 * STATIC VARIABLES
 ********************************/
static TimerHandle_t s_tmr; /* handle of heart beat timer */

/*********************************
 * FORWARD DECLARATIONS
 ********************************/
static void bt_app_av_state_unconnected_hdlr(uint16_t event, void *param);
static void bt_app_av_state_connecting_hdlr(uint16_t event, void *param);
static void bt_app_av_state_connected_hdlr(uint16_t event, void *param);
static void bt_app_av_state_disconnecting_hdlr(uint16_t event, void *param);
static void bt_app_av_media_proc(uint16_t event, void *param);

/*********************************
 * STATIC FUNCTIONS
 ********************************/
static void bt_app_a2d_heart_beat(TimerHandle_t arg) {
  bt_app_work_dispatch(bt_a2dp_sm_handler, BT_APP_HEART_BEAT_EVT, NULL, 0,
                       NULL);
}

static void bt_app_av_state_unconnected_hdlr(uint16_t event, void *param) {
  esp_a2d_cb_param_t *a2d = NULL;
  /* handle the events of interest in unconnected state */
  switch (event) {
  case ESP_A2D_CONNECTION_STATE_EVT:
  case ESP_A2D_AUDIO_STATE_EVT:
  case ESP_A2D_AUDIO_CFG_EVT:
  case ESP_A2D_MEDIA_CTRL_ACK_EVT:
    break;
  case BT_APP_HEART_BEAT_EVT: {
    uint8_t *bda = s_peer_bda;
    ESP_LOGI(BT_AV_TAG,
             "a2dp connecting to peer: %02x:%02x:%02x:%02x:%02x:%02x", bda[0],
             bda[1], bda[2], bda[3], bda[4], bda[5]);
    esp_a2d_source_connect(s_peer_bda);
    s_a2d_state = APP_AV_STATE_CONNECTING;
    s_connecting_intv = 0;
    break;
  }
  case ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT: {
    a2d = (esp_a2d_cb_param_t *)(param);
    ESP_LOGI(BT_AV_TAG, "%s, delay value: %u * 1/10 ms", __func__,
             a2d->a2d_report_delay_value_stat.delay_value);
    break;
  }
  default: {
    ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
    break;
  }
  }
}

static void bt_app_av_state_connecting_hdlr(uint16_t event, void *param) {
  esp_a2d_cb_param_t *a2d = NULL;

  /* handle the events of interest in connecting state */
  switch (event) {
  case ESP_A2D_CONNECTION_STATE_EVT: {
    a2d = (esp_a2d_cb_param_t *)(param);
    if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
      ESP_LOGI(BT_AV_TAG, "a2dp connected");
      s_a2d_state = APP_AV_STATE_CONNECTED;
      s_media_state = APP_AV_MEDIA_STATE_IDLE;
    } else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
      s_a2d_state = APP_AV_STATE_UNCONNECTED;
    }
    break;
  }
  case ESP_A2D_AUDIO_STATE_EVT:
  case ESP_A2D_AUDIO_CFG_EVT:
  case ESP_A2D_MEDIA_CTRL_ACK_EVT:
    break;
  case BT_APP_HEART_BEAT_EVT:
    /**
     * Switch state to APP_AV_STATE_UNCONNECTED
     * when connecting lasts more than 2 heart beat intervals.
     */
    if (++s_connecting_intv >= 2) {
      s_a2d_state = APP_AV_STATE_UNCONNECTED;
      s_connecting_intv = 0;
    }
    break;
  case ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT: {
    a2d = (esp_a2d_cb_param_t *)(param);
    ESP_LOGI(BT_AV_TAG, "%s, delay value: %u * 1/10 ms", __func__,
             a2d->a2d_report_delay_value_stat.delay_value);
    break;
  }
  default:
    ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
    break;
  }
}

static void bt_app_av_media_proc(uint16_t event, void *param) {
  esp_a2d_cb_param_t *a2d = NULL;

  switch (s_media_state) {
  case APP_AV_MEDIA_STATE_IDLE: {
    if (event == BT_APP_HEART_BEAT_EVT) {
      ESP_LOGI(BT_AV_TAG, "a2dp media ready checking ...");
      esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
    } else if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
      a2d = (esp_a2d_cb_param_t *)(param);
      if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY &&
          a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
        ESP_LOGI(BT_AV_TAG, "a2dp media ready, starting ...");
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
        s_media_state = APP_AV_MEDIA_STATE_STARTING;
      }
    }
    break;
  }
  case APP_AV_MEDIA_STATE_STARTING: {
    if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
      a2d = (esp_a2d_cb_param_t *)(param);
      if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START &&
          a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
        ESP_LOGI(BT_AV_TAG, "a2dp media start successfully.");
        s_intv_cnt = 0;
        s_media_state = APP_AV_MEDIA_STATE_STARTED;
      } else {
        /* not started successfully, transfer to idle state */
        ESP_LOGI(BT_AV_TAG, "a2dp media start failed.");
        s_media_state = APP_AV_MEDIA_STATE_IDLE;
      }
    }
    break;
  }
  case APP_AV_MEDIA_STATE_STARTED: {
    if (event == BT_APP_HEART_BEAT_EVT) {
      // Removed auto-suspend logic to allow continuous playback
    }
    break;
  }
  case APP_AV_MEDIA_STATE_STOPPING: {
    if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
      a2d = (esp_a2d_cb_param_t *)(param);
      if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_SUSPEND &&
          a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
        ESP_LOGI(BT_AV_TAG,
                 "a2dp media suspend successfully, disconnecting...");
        s_media_state = APP_AV_MEDIA_STATE_IDLE;
        esp_a2d_source_disconnect(s_peer_bda);
        s_a2d_state = APP_AV_STATE_DISCONNECTING;
      } else {
        ESP_LOGI(BT_AV_TAG, "a2dp media suspending...");
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
      }
    }
    break;
  }
  default: {
    break;
  }
  }
}

static void bt_app_av_state_connected_hdlr(uint16_t event, void *param) {
  esp_a2d_cb_param_t *a2d = NULL;

  /* handle the events of interest in connected state */
  switch (event) {
  case ESP_A2D_CONNECTION_STATE_EVT: {
    a2d = (esp_a2d_cb_param_t *)(param);
    if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
      ESP_LOGI(BT_AV_TAG, "a2dp disconnected");
      s_a2d_state = APP_AV_STATE_UNCONNECTED;
    }
    break;
  }
  case ESP_A2D_AUDIO_STATE_EVT: {
    a2d = (esp_a2d_cb_param_t *)(param);
    if (ESP_A2D_AUDIO_STATE_STARTED == a2d->audio_stat.state) {
      s_pkt_cnt = 0;
    }
    break;
  }
  case ESP_A2D_AUDIO_CFG_EVT:
    // not supposed to occur for A2DP source
    break;
  case ESP_A2D_MEDIA_CTRL_ACK_EVT:
  case BT_APP_HEART_BEAT_EVT: {
    bt_app_av_media_proc(event, param);
    break;
  }
  case ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT: {
    a2d = (esp_a2d_cb_param_t *)(param);
    ESP_LOGI(BT_AV_TAG, "%s, delay value: %u * 1/10 ms", __func__,
             a2d->a2d_report_delay_value_stat.delay_value);
    break;
  }
  default: {
    ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
    break;
  }
  }
}

static void bt_app_av_state_disconnecting_hdlr(uint16_t event, void *param) {
  esp_a2d_cb_param_t *a2d = NULL;

  /* handle the events of interest in disconnecing state */
  switch (event) {
  case ESP_A2D_CONNECTION_STATE_EVT: {
    a2d = (esp_a2d_cb_param_t *)(param);
    if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
      ESP_LOGI(BT_AV_TAG, "a2dp disconnected");
      s_a2d_state = APP_AV_STATE_UNCONNECTED;
    }
    break;
  }
  case ESP_A2D_AUDIO_STATE_EVT:
  case ESP_A2D_AUDIO_CFG_EVT:
  case ESP_A2D_MEDIA_CTRL_ACK_EVT:
  case BT_APP_HEART_BEAT_EVT:
    break;
  case ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT: {
    a2d = (esp_a2d_cb_param_t *)(param);
    ESP_LOGI(BT_AV_TAG, "%s, delay value: 0x%u * 1/10 ms", __func__,
             a2d->a2d_report_delay_value_stat.delay_value);
    break;
  }
  default: {
    ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
    break;
  }
  }
}

/*********************************
 * PUBLIC FUNCTIONS
 ********************************/
void bt_a2dp_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
  bt_app_work_dispatch(bt_a2dp_sm_handler, event, param,
                       sizeof(esp_a2d_cb_param_t), NULL);
}

int32_t bt_a2dp_data_callback(uint8_t *data, int32_t len) {
  return audio_player_get_data(data, len);
}

void bt_a2dp_sm_handler(uint16_t event, void *param) {
  ESP_LOGI(BT_AV_TAG, "%s state: %d, event: 0x%x", __func__, s_a2d_state,
           event);

  /* select handler according to different states */
  switch (s_a2d_state) {
  case APP_AV_STATE_DISCOVERING:
  case APP_AV_STATE_DISCOVERED:
    break;
  case APP_AV_STATE_UNCONNECTED:
    bt_app_av_state_unconnected_hdlr(event, param);
    break;
  case APP_AV_STATE_CONNECTING:
    bt_app_av_state_connecting_hdlr(event, param);
    break;
  case APP_AV_STATE_CONNECTED:
    bt_app_av_state_connected_hdlr(event, param);
    break;
  case APP_AV_STATE_DISCONNECTING:
    bt_app_av_state_disconnecting_hdlr(event, param);
    break;
  default:
    ESP_LOGE(BT_AV_TAG, "%s invalid state: %d", __func__, s_a2d_state);
    break;
  }
}

void bt_a2dp_init(void) {
  esp_a2d_source_init();
  esp_a2d_register_callback(&bt_a2dp_callback);
  esp_a2d_source_register_data_callback(bt_a2dp_data_callback);

  /* create and start heart beat timer */
  int tmr_id = 0;
  s_tmr = xTimerCreate("connTmr", (10000 / portTICK_PERIOD_MS), pdTRUE,
                       (void *)&tmr_id, bt_app_a2d_heart_beat);
  xTimerStart(s_tmr, portMAX_DELAY);
}

TimerHandle_t bt_a2dp_get_timer(void) { return s_tmr; }
