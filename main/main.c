/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <dirent.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bt_app_core.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"

#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "freertos/ringbuf.h"
#include "sdmmc_cmd.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#include "minimp3.h"

/* log tags */
#define BT_AV_TAG "BT_AV"
#define BT_RC_CT_TAG "RC_CT"

/* device name */
#define LOCAL_DEVICE_NAME "ESP_A2DP_SRC"

/* AVRCP used transaction label */
#define APP_RC_CT_TL_GET_CAPS (0)
#define APP_RC_CT_TL_RN_VOLUME_CHANGE (1)

enum {
  BT_APP_STACK_UP_EVT = 0x0000,   /* event for stack up */
  BT_APP_HEART_BEAT_EVT = 0xff00, /* event for heart beat */
};

/* A2DP global states */
enum {
  APP_AV_STATE_IDLE,
  APP_AV_STATE_DISCOVERING,
  APP_AV_STATE_DISCOVERED,
  APP_AV_STATE_UNCONNECTED,
  APP_AV_STATE_CONNECTING,
  APP_AV_STATE_CONNECTED,
  APP_AV_STATE_DISCONNECTING,
};

/* sub states of APP_AV_STATE_CONNECTED */
enum {
  APP_AV_MEDIA_STATE_IDLE,
  APP_AV_MEDIA_STATE_STARTING,
  APP_AV_MEDIA_STATE_STARTED,
  APP_AV_MEDIA_STATE_STOPPING,
};

/*********************************
 * STATIC FUNCTION DECLARATIONS
 ********************************/

/* handler for bluetooth stack enabled events */
static void bt_av_hdl_stack_evt(uint16_t event, void *p_param);

/* avrc controller event handler */
static void bt_av_hdl_avrc_ct_evt(uint16_t event, void *p_param);

/* GAP callback function */
static void bt_app_gap_cb(esp_bt_gap_cb_event_t event,
                          esp_bt_gap_cb_param_t *param);

/* callback function for A2DP source */
static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);

/* callback function for A2DP source audio data stream */
static int32_t bt_app_a2d_data_cb(uint8_t *data, int32_t len);

/* callback function for AVRCP controller */
static void bt_app_rc_ct_cb(esp_avrc_ct_cb_event_t event,
                            esp_avrc_ct_cb_param_t *param);

/* handler for heart beat timer */
static void bt_app_a2d_heart_beat(TimerHandle_t arg);

/* A2DP application state machine */
static void bt_app_av_sm_hdlr(uint16_t event, void *param);

/* utils for transfer BLuetooth Deveice Address into string form */
static char *bda2str(esp_bd_addr_t bda, char *str, size_t size);

/* A2DP application state machine handler for each state */
static void bt_app_av_state_unconnected_hdlr(uint16_t event, void *param);
static void bt_app_av_state_connecting_hdlr(uint16_t event, void *param);
static void bt_app_av_state_connected_hdlr(uint16_t event, void *param);
static void bt_app_av_state_disconnecting_hdlr(uint16_t event, void *param);

/*********************************
 * STATIC VARIABLE DEFINITIONS
 ********************************/

static esp_bd_addr_t s_peer_bda = {
    0}; /* Bluetooth Device Address of peer device*/
static uint8_t s_peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN +
                             1]; /* Bluetooth Device Name of peer device*/
static int s_a2d_state = APP_AV_STATE_IDLE; /* A2DP global state */
static int s_media_state =
    APP_AV_MEDIA_STATE_IDLE; /* sub states of APP_AV_STATE_CONNECTED */
static int s_intv_cnt = 0;   /* count of heart beat intervals */
static int s_connecting_intv =
    0; /* count of heart beat intervals for connecting */
static uint32_t s_pkt_cnt = 0; /* count of packets */
static esp_avrc_rn_evt_cap_mask_t
    s_avrc_peer_rn_cap; /* AVRC target notification event capability bit mask */
static TimerHandle_t s_tmr; /* handle of heart beat timer */

static RingbufHandle_t s_ringbuf_handle = NULL;
static mp3dec_t s_mp3d;

#define MOUNT_POINT "/sdcard"
#define RINGBUF_SIZE (8 * 1024)

static const char remote_device_name[] = CONFIG_EXAMPLE_PEER_DEVICE_NAME;

/*********************************
 * STATIC FUNCTION DEFINITIONS
 ********************************/

static char *bda2str(esp_bd_addr_t bda, char *str, size_t size) {
  if (bda == NULL || str == NULL || size < 18) {
    return NULL;
  }

  sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x", bda[0], bda[1], bda[2], bda[3],
          bda[4], bda[5]);
  return str;
}

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

static void bt_app_gap_cb(esp_bt_gap_cb_event_t event,
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

static void bt_av_hdl_stack_evt(uint16_t event, void *p_param) {
  ESP_LOGD(BT_AV_TAG, "%s event: %d", __func__, event);

  switch (event) {
  /* when stack up worked, this event comes */
  case BT_APP_STACK_UP_EVT: {
    char *dev_name = LOCAL_DEVICE_NAME;
    esp_bt_gap_set_device_name(dev_name);
    esp_bt_gap_register_callback(bt_app_gap_cb);

    esp_avrc_ct_init();
    esp_avrc_ct_register_callback(bt_app_rc_ct_cb);

    esp_avrc_rn_evt_cap_mask_t evt_set = {0};
    esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set,
                                       ESP_AVRC_RN_VOLUME_CHANGE);
    ESP_ERROR_CHECK(esp_avrc_tg_set_rn_evt_cap(&evt_set));

    esp_a2d_source_init();
    esp_a2d_register_callback(&bt_app_a2d_cb);
    esp_a2d_source_register_data_callback(bt_app_a2d_data_cb);

    /* Avoid the state error of s_a2d_state caused by the connection initiated
     * by the peer device. */
    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    esp_bt_gap_get_device_name();

    ESP_LOGI(BT_AV_TAG, "Starting device discovery...");
    s_a2d_state = APP_AV_STATE_DISCOVERING;
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);

    /* create and start heart beat timer */
    do {
      int tmr_id = 0;
      s_tmr = xTimerCreate("connTmr", (10000 / portTICK_PERIOD_MS), pdTRUE,
                           (void *)&tmr_id, bt_app_a2d_heart_beat);
      xTimerStart(s_tmr, portMAX_DELAY);
    } while (0);
    break;
  }
  /* other */
  default: {
    ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
    break;
  }
  }
}

static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
  bt_app_work_dispatch(bt_app_av_sm_hdlr, event, param,
                       sizeof(esp_a2d_cb_param_t), NULL);
}

/* generate 1kHz sine wave to simulate source audio */
static int32_t bt_app_a2d_data_cb(uint8_t *data, int32_t len) {
  if (data == NULL || len < 0) {
    return 0;
  }

  if (s_ringbuf_handle) {
    int32_t bytes_filled = 0;
    while (bytes_filled < len) {
      size_t item_size;
      // Receive up to the remaining needed bytes
      char *item = (char *)xRingbufferReceiveUpTo(s_ringbuf_handle, &item_size,
                                                  0, len - bytes_filled);
      if (item != NULL) {
        memcpy(data + bytes_filled, item, item_size);
        vRingbufferReturnItem(s_ringbuf_handle, (void *)item);
        bytes_filled += item_size;
      } else {
        // No more data available right now
        break;
      }
    }

    // If we didn't fill the buffer, pad with silence
    if (bytes_filled < len) {
      memset(data + bytes_filled, 0, len - bytes_filled);
    }
    return len;
  }

  // Silence if no data
  memset(data, 0, len);
  return len;
}

static void bt_app_a2d_heart_beat(TimerHandle_t arg) {
  bt_app_work_dispatch(bt_app_av_sm_hdlr, BT_APP_HEART_BEAT_EVT, NULL, 0, NULL);
}

static void bt_app_av_sm_hdlr(uint16_t event, void *param) {
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
      /* stop media after 10 heart beat intervals */
      if (++s_intv_cnt >= 10) {
        ESP_LOGI(BT_AV_TAG, "a2dp media suspending...");
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
        s_media_state = APP_AV_MEDIA_STATE_STOPPING;
        s_intv_cnt = 0;
      }
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

/* callback function for AVRCP controller */
static void bt_app_rc_ct_cb(esp_avrc_ct_cb_event_t event,
                            esp_avrc_ct_cb_param_t *param) {
  switch (event) {
  case ESP_AVRC_CT_CONNECTION_STATE_EVT:
  case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
  case ESP_AVRC_CT_METADATA_RSP_EVT:
  case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
  case ESP_AVRC_CT_REMOTE_FEATURES_EVT:
  case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT:
  case ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT: {
    bt_app_work_dispatch(bt_av_hdl_avrc_ct_evt, event, param,
                         sizeof(esp_avrc_ct_cb_param_t), NULL);
    break;
  }
  default: {
    ESP_LOGE(BT_RC_CT_TAG, "Invalid AVRC event: %d", event);
    break;
  }
  }
}

static void bt_av_volume_changed(void) {
  if (esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_TEST,
                                         &s_avrc_peer_rn_cap,
                                         ESP_AVRC_RN_VOLUME_CHANGE)) {
    esp_avrc_ct_send_register_notification_cmd(APP_RC_CT_TL_RN_VOLUME_CHANGE,
                                               ESP_AVRC_RN_VOLUME_CHANGE, 0);
  }
}

void bt_av_notify_evt_handler(uint8_t event_id,
                              esp_avrc_rn_param_t *event_parameter) {
  switch (event_id) {
  /* when volume changed locally on target, this event comes */
  case ESP_AVRC_RN_VOLUME_CHANGE: {
    ESP_LOGI(BT_RC_CT_TAG, "Volume changed: %d", event_parameter->volume);
    ESP_LOGI(BT_RC_CT_TAG, "Set absolute volume: volume %d",
             event_parameter->volume + 5);
    esp_avrc_ct_send_set_absolute_volume_cmd(APP_RC_CT_TL_RN_VOLUME_CHANGE,
                                             event_parameter->volume + 5);
    bt_av_volume_changed();
    break;
  }
  /* other */
  default:
    break;
  }
}

/* AVRC controller event handler */
static void bt_av_hdl_avrc_ct_evt(uint16_t event, void *p_param) {
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

/*********************************
 * MAIN ENTRY POINT
 ********************************/

static void init_sd_card(void) {
  esp_err_t ret;
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024};
  sdmmc_card_t *card;
  const char mount_point[] = MOUNT_POINT;
  ESP_LOGI(BT_AV_TAG, "Initializing SD card");

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.max_freq_khz = 400; // Lower frequency for better stability
  spi_bus_config_t bus_cfg = {
      .mosi_io_num = 26,
      .miso_io_num = 19,
      .sclk_io_num = 18,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 4000,
  };
  ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
  if (ret != ESP_OK) {
    ESP_LOGE(BT_AV_TAG, "Failed to initialize bus.");
    return;
  }

  // Enable internal pull-ups for MISO (often needed if no external pull-up)
  gpio_set_pull_mode(19, GPIO_PULLUP_ONLY);

  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = 27;
  slot_config.host_id = host.slot;

  ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config,
                                &card);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(BT_AV_TAG, "Failed to mount filesystem.");
    } else {
      ESP_LOGE(BT_AV_TAG, "Failed to initialize the card (%s).",
               esp_err_to_name(ret));
    }
    return;
  }
  ESP_LOGI(BT_AV_TAG, "Filesystem mounted");
  sdmmc_card_print_info(stdout, card);
}

static void mp3_decode_task(void *arg) {
  DIR *dir = opendir(MOUNT_POINT);
  if (!dir) {
    ESP_LOGE(BT_AV_TAG, "Failed to open directory");
    vTaskDelete(NULL);
    return;
  }

  struct dirent *entry;
  char filepath[512];
  bool found = false;

  while ((entry = readdir(dir)) != NULL) {
    if (strstr(entry->d_name, ".mp3") || strstr(entry->d_name, ".MP3")) {
      snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, entry->d_name);
      ESP_LOGI(BT_AV_TAG, "Found mp3 file: %s", filepath);
      found = true;
      break;
    }
  }
  closedir(dir);

  if (!found) {
    ESP_LOGE(BT_AV_TAG, "No MP3 file found");
    vTaskDelete(NULL);
    return;
  }

  FILE *f = fopen(filepath, "rb");
  if (!f) {
    ESP_LOGE(BT_AV_TAG, "Failed to open file");
    vTaskDelete(NULL);
    return;
  }

  mp3dec_init(&s_mp3d);
  // read_buf was unused, removing it.
  int16_t *pcm_buf = malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(int16_t));

  if (!pcm_buf) {
    ESP_LOGE(BT_AV_TAG, "Failed to allocate pcm buffer");
    fclose(f);
    vTaskDelete(NULL);
    return;
  }

  // Simple decoding loop (naive implementation)
  // In a real app, we need to handle ID3 tags and proper streaming
  // minimp3 mp3dec_ex functions are better for file streaming, but here we use
  // low level for control Actually, let's use mp3dec_ex_open if available? No,
  // I included minimp3.h which is low level. Wait, minimp3_ex.h is separate. I
  // only downloaded minimp3.h. I will use the low level API.

// We need a buffer for input stream
#define INPUT_BUF_SIZE (4 * 1024)
  uint8_t *input_buf = malloc(INPUT_BUF_SIZE);
  if (!input_buf) {
    ESP_LOGE(BT_AV_TAG, "Failed to allocate input buffer");
    free(pcm_buf);
    fclose(f);
    vTaskDelete(NULL);
    return;
  }
  int buf_valid = 0;

  while (1) {
    if (buf_valid < INPUT_BUF_SIZE) {
      int read = fread(input_buf + buf_valid, 1, INPUT_BUF_SIZE - buf_valid, f);
      if (read == 0) {
        // End of file, rewind
        fseek(f, 0, SEEK_SET);
        continue;
      }
      buf_valid += read;
    }

    mp3dec_frame_info_t info;
    int samples =
        mp3dec_decode_frame(&s_mp3d, input_buf, buf_valid, pcm_buf, &info);

    if (samples > 0) {
      static bool s_format_logged = false;
      if (!s_format_logged) {
        ESP_LOGI(BT_AV_TAG, "MP3 format: %d Hz, %d channels", info.hz,
                 info.channels);
        s_format_logged = true;
      }
      // We have PCM data
      size_t pcm_size = samples * info.channels * 2;
      // Send to ringbuffer, wait indefinitely if full
      if (xRingbufferSend(s_ringbuf_handle, pcm_buf, pcm_size, portMAX_DELAY) !=
          pdTRUE) {
        ESP_LOGW(BT_AV_TAG, "Ringbuffer send failed");
      }
    }

    // Move remaining data to start
    int consumed = info.frame_bytes;
    if (consumed > 0 && consumed <= buf_valid) {
      memmove(input_buf, input_buf + consumed, buf_valid - consumed);
      buf_valid -= consumed;
    } else if (consumed == 0) {
      // Error or need more data
      if (buf_valid == INPUT_BUF_SIZE) {
        // Skip one byte to try resync
        memmove(input_buf, input_buf + 1, buf_valid - 1);
        buf_valid--;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1)); // Yield
  }

  free(input_buf);
  free(pcm_buf);
  fclose(f);
  vTaskDelete(NULL);
}

void app_main(void) {
  char bda_str[18] = {0};

  s_ringbuf_handle = xRingbufferCreate(RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
  init_sd_card();
  xTaskCreate(mp3_decode_task, "mp3_decode", 32 * 1024, NULL, 5, NULL);
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
