/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "oled_display.h"
#include "common.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sd_card.h"
#include "ssd1306.h"
#include <stdio.h>
#include <string.h>

/*********************************
 * CONFIGURATION
 ********************************/
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000

#define OLED_TAG "OLED"

/*********************************
 * STATIC VARIABLES
 ********************************/
static bool s_oled_initialized = false;
static SSD1306_t s_oled_dev;

/*********************************
 * HELPER FUNCTIONS
 ********************************/

/**
 * @brief Get bluetooth connection state string
 */
static const char *get_bt_state_str(void) {
  switch (s_a2d_state) {
  case APP_AV_STATE_IDLE:
    return "IDLE";
  case APP_AV_STATE_DISCOVERING:
    return "SCAN";
  case APP_AV_STATE_DISCOVERED:
    return "FOUND";
  case APP_AV_STATE_CONNECTING:
    return "CONN";
  case APP_AV_STATE_CONNECTED:
    if (s_media_state == APP_AV_MEDIA_STATE_STARTED) {
      return "BT";
    }
    return "BT";
  case APP_AV_STATE_DISCONNECTING:
    return "DISC";
  default:
    return "----";
  }
}

/**
 * @brief Extract filename from full path
 */
static void get_filename(const char *path, char *filename, size_t max_len) {
  if (!path || !filename) {
    return;
  }

  const char *last_slash = strrchr(path, '/');
  const char *name = last_slash ? last_slash + 1 : path;

  // Remove .mp3 extension
  const char *ext = strrchr(name, '.');
  size_t name_len = ext ? (size_t)(ext - name) : strlen(name);

  if (name_len >= max_len) {
    name_len = max_len - 1;
  }

  strncpy(filename, name, name_len);
  filename[name_len] = '\0';
}

/*********************************
 * PUBLIC FUNCTIONS
 ********************************/

void oled_display_init(void) {
  ESP_LOGI(OLED_TAG, "Initializing SSD1306 OLED (128x32)");
  ESP_LOGI(OLED_TAG, "I2C: SDA=%d, SCL=%d", I2C_MASTER_SDA_IO,
           I2C_MASTER_SCL_IO);

  // Initialize I2C and SSD1306 using new driver
  i2c_master_init(&s_oled_dev, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, -1);
  i2c_init(&s_oled_dev, 128, 32);

  // Clear display and show startup message
  ssd1306_clear_screen(&s_oled_dev, false);
  ssd1306_contrast(&s_oled_dev, 0xFF);

  ssd1306_display_text(&s_oled_dev, 0, "ESP32 A2DP SRC", 14, false);
  ssd1306_display_text(&s_oled_dev, 2, "Initializing...", 15, false);

  s_oled_initialized = true;
  ESP_LOGI(OLED_TAG, "OLED initialized successfully");
}

void oled_display_clear(void) {
  if (!s_oled_initialized) {
    return;
  }
  ssd1306_clear_screen(&s_oled_dev, false);
}

void oled_display_update(void) {
  if (!s_oled_initialized) {
    return;
  }

  char line1[32] = {0};
  char line2[32] = {0};

  /******************************************
   * LINE 1: [BT] Track/Total  Song Name
   ******************************************/
  const char *bt_status = get_bt_state_str();
  int total_songs = sd_card_get_playlist_count();

  // Get current song info (keep small to avoid truncation warnings)
  char song_name[10] = "NoSong";
  extern int s_current_song_idx; // From audio_player.c

  if (total_songs > 0) {
    const char *file_path = sd_card_get_file_path(s_current_song_idx);
    if (file_path) {
      get_filename(file_path, song_name, sizeof(song_name));
    }
// Disable truncation warning - we've sized buffers appropriately
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(line1, sizeof(line1), "[%s]%d/%d %s", bt_status,
             s_current_song_idx + 1, total_songs, song_name);
#pragma GCC diagnostic pop
  } else {
    snprintf(line1, sizeof(line1), "[%s] No Files", bt_status);
  }

  /******************************************
   * LINE 2: Progress bar + Volume
   ******************************************/
  // Create a simple progress bar (12 chars) + volume
  char progress_bar[13] = "          "; // 10 spaces + null terminator
  int bar_len = 10;

  // Calculate progress based on playing state
  static int progress_pos = 0;
  if (s_is_playing) {
    progress_pos = (progress_pos + 1) % (bar_len + 1);
  }

  // Fill progress bar
  for (int i = 0; i < bar_len; i++) {
    if (i < progress_pos) {
      progress_bar[i] = '=';
    } else if (i == progress_pos) {
      progress_bar[i] = '>';
    } else {
      progress_bar[i] = '-';
    }
  }

  // Add play/pause indicator and volume
  char play_icon = s_is_playing ? '>' : '|';
  snprintf(line2, sizeof(line2), "%c%s V:%d", play_icon, progress_bar,
           (s_current_volume * 100) / 127); // Convert to 0-100

  /******************************************
   * Update display
   ******************************************/
  ssd1306_display_text(&s_oled_dev, 0, line1, strlen(line1), false);
  ssd1306_display_text(&s_oled_dev, 2, line2, strlen(line2), false);
}
