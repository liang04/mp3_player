/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include "esp_avrc_api.h"
#include "esp_bt.h"
#include "esp_gap_bt_api.h"
#include <stdbool.h>
#include <stdint.h>

/*********************************
 * LOG TAGS
 ********************************/
#define BT_AV_TAG "BT_AV"
#define BT_RC_CT_TAG "RC_CT"

/*********************************
 * DEVICE CONFIGURATION
 ********************************/
#define LOCAL_DEVICE_NAME "ESP_A2DP_SRC"

/*********************************
 * AVRCP CONFIGURATION
 ********************************/
#define APP_RC_CT_TL_GET_CAPS (0)
#define APP_RC_CT_TL_RN_VOLUME_CHANGE (1)

/*********************************
 * EVENTS
 ********************************/
enum {
  BT_APP_STACK_UP_EVT = 0x0000,   /* event for stack up */
  BT_APP_HEART_BEAT_EVT = 0xff00, /* event for heart beat */
};

/*********************************
 * A2DP STATES
 ********************************/
enum {
  APP_AV_STATE_IDLE,
  APP_AV_STATE_DISCOVERING,
  APP_AV_STATE_DISCOVERED,
  APP_AV_STATE_UNCONNECTED,
  APP_AV_STATE_CONNECTING,
  APP_AV_STATE_CONNECTED,
  APP_AV_STATE_DISCONNECTING,
};

/*********************************
 * MEDIA STATES
 ********************************/
enum {
  APP_AV_MEDIA_STATE_IDLE,
  APP_AV_MEDIA_STATE_STARTING,
  APP_AV_MEDIA_STATE_STARTED,
  APP_AV_MEDIA_STATE_STOPPING,
};

/*********************************
 * GLOBAL VARIABLES (declared extern, defined in respective modules)
 ********************************/

/* Bluetooth GAP - defined in bt_gap.c */
extern esp_bd_addr_t s_peer_bda;
extern uint8_t s_peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];

/* A2DP - defined in bt_a2dp.c */
extern int s_a2d_state;
extern int s_media_state;
extern int s_intv_cnt;
extern int s_connecting_intv;
extern uint32_t s_pkt_cnt;

/* AVRCP - defined in bt_avrcp.c */
extern esp_avrc_rn_evt_cap_mask_t s_avrc_peer_rn_cap;
extern bool s_volume_init_done;

/* Audio Player - defined in audio_player.c */
extern uint8_t s_current_volume;
extern bool s_is_playing;
extern bool s_next_song_req;
extern bool s_prev_song_req;
extern int s_current_song_idx;

/*********************************
 * UTILITY FUNCTIONS
 ********************************/

/**
 * @brief Convert Bluetooth Device Address to string
 * @param bda Bluetooth Device Address
 * @param str Output string buffer
 * @param size Size of output buffer (should be at least 18)
 * @return Pointer to the string, or NULL on error
 */
char *bda2str(esp_bd_addr_t bda, char *str, size_t size);

#endif /* __COMMON_H__ */
