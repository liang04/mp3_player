/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "audio_player.h"
#include "common.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "sd_card.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#include "minimp3.h"

/*********************************
 * CONSTANTS
 ********************************/
#define RINGBUF_SIZE (32 * 1024) // 32KB for smoother playback
#define INPUT_BUF_SIZE (4 * 1024)

/*********************************
 * MODULE VARIABLES
 ********************************/
uint8_t s_current_volume = 20; // Default initial volume
bool s_is_playing = true;
bool s_next_song_req = false;
bool s_prev_song_req = false;

/*********************************
 * STATIC VARIABLES
 ********************************/
static RingbufHandle_t s_ringbuf_handle = NULL;
static mp3dec_t s_mp3d;
static int s_current_song_idx = 0;

/*********************************
 * STATIC FUNCTIONS
 ********************************/
static void mp3_decode_task(void *arg) {
  sd_card_scan_playlist();

  if (sd_card_get_playlist_count() == 0) {
    ESP_LOGE(BT_AV_TAG, "No MP3 files found");
    vTaskDelete(NULL);
    return;
  }

  mp3dec_init(&s_mp3d);
  int16_t *pcm_buf = malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(int16_t));
  if (!pcm_buf) {
    ESP_LOGE(BT_AV_TAG, "Failed to allocate pcm buffer");
    vTaskDelete(NULL);
    return;
  }

  uint8_t *input_buf = malloc(INPUT_BUF_SIZE);
  if (!input_buf) {
    ESP_LOGE(BT_AV_TAG, "Failed to allocate input buffer");
    free(pcm_buf);
    vTaskDelete(NULL);
    return;
  }

  while (1) {
    // Open current file
    const char *file_path = sd_card_get_file_path(s_current_song_idx);
    if (!file_path) {
      ESP_LOGE(BT_AV_TAG, "Invalid song index");
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    ESP_LOGI(BT_AV_TAG, "Playing: %s", file_path);
    FILE *f = fopen(file_path, "rb");
    if (!f) {
      ESP_LOGE(BT_AV_TAG, "Failed to open file");
      // Try next one
      s_current_song_idx =
          (s_current_song_idx + 1) % sd_card_get_playlist_count();
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    mp3dec_init(&s_mp3d); // Reset decoder state for new file
    int buf_valid = 0;
    bool file_done = false;

    while (!file_done) {
      // Check control flags
      if (s_next_song_req) {
        s_next_song_req = false;
        s_current_song_idx =
            (s_current_song_idx + 1) % sd_card_get_playlist_count();
        file_done = true; // Break inner loop to reload
        break;
      }
      if (s_prev_song_req) {
        s_prev_song_req = false;
        s_current_song_idx =
            (s_current_song_idx - 1 + sd_card_get_playlist_count()) %
            sd_card_get_playlist_count();
        file_done = true;
        break;
      }

      if (!s_is_playing) {
        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
      }

      if (buf_valid < INPUT_BUF_SIZE) {
        int read =
            fread(input_buf + buf_valid, 1, INPUT_BUF_SIZE - buf_valid, f);
        if (read == 0) {
          // End of file, go to next song
          s_current_song_idx =
              (s_current_song_idx + 1) % sd_card_get_playlist_count();
          file_done = true;
          break;
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
        if (xRingbufferSend(s_ringbuf_handle, pcm_buf, pcm_size,
                            portMAX_DELAY) != pdTRUE) {
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
        // Yield to prevent tight loop when waiting for more data
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }

    fclose(f);
  }

  free(input_buf);
  free(pcm_buf);
  vTaskDelete(NULL);
}

/*********************************
 * PUBLIC FUNCTIONS
 ********************************/
void audio_player_init(void) {
  s_ringbuf_handle = xRingbufferCreate(RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
  if (!s_ringbuf_handle) {
    ESP_LOGE(BT_AV_TAG, "Failed to create ringbuffer");
    return;
  }

  xTaskCreate(mp3_decode_task, "mp3_decode", 32 * 1024, NULL, 5, NULL);
}

int32_t audio_player_get_data(uint8_t *data, int32_t len) {
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

    // Apply software volume control to the PCM data
    if (s_current_volume < 127 && bytes_filled > 0) {
      // Calculate volume scale factor (0.0 to 1.0)
      float volume_scale = (float)s_current_volume / 127.0f;

      // Apply volume to 16-bit PCM samples
      int16_t *samples = (int16_t *)data;
      int32_t sample_count = bytes_filled / 2; // 16-bit = 2 bytes per sample

      for (int32_t i = 0; i < sample_count; i++) {
        // Scale the sample and clamp to prevent overflow
        int32_t scaled = (int32_t)(samples[i] * volume_scale);
        if (scaled > 32767)
          scaled = 32767;
        if (scaled < -32768)
          scaled = -32768;
        samples[i] = (int16_t)scaled;
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

void audio_player_set_volume(uint8_t volume) {
  if (volume > 127) {
    volume = 127;
  }
  s_current_volume = volume;
  ESP_LOGI(BT_AV_TAG, "Volume set to %d", volume);
}

uint8_t audio_player_get_volume(void) { return s_current_volume; }
