/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "button_control.h"
#include "common.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*********************************
 * GPIO CONFIGURATION
 ********************************/
#define GPIO_BTN_PREV 4
#define GPIO_BTN_PLAY 5
#define GPIO_BTN_NEXT 15

/*********************************
 * STATIC FUNCTIONS
 ********************************/
static void button_task(void *arg) {
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pin_bit_mask = (1ULL << GPIO_BTN_PREV) | (1ULL << GPIO_BTN_PLAY) |
                         (1ULL << GPIO_BTN_NEXT);
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 1;
  gpio_config(&io_conf);

  int last_play_state = 1;
  int last_next_state = 1;
  int last_prev_state = 1;

  while (1) {
    int play_state = gpio_get_level(GPIO_BTN_PLAY);
    int next_state = gpio_get_level(GPIO_BTN_NEXT);
    int prev_state = gpio_get_level(GPIO_BTN_PREV);

    if (last_play_state == 1 && play_state == 0) {
      s_is_playing = !s_is_playing;
      ESP_LOGI(BT_AV_TAG, "Button: Play/Pause -> %s",
               s_is_playing ? "Playing" : "Paused");
      vTaskDelay(pdMS_TO_TICKS(200)); // Debounce
    }

    if (last_next_state == 1 && next_state == 0) {
      s_next_song_req = true;
      ESP_LOGI(BT_AV_TAG, "Button: Next");
      vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (last_prev_state == 1 && prev_state == 0) {
      s_prev_song_req = true;
      ESP_LOGI(BT_AV_TAG, "Button: Prev");
      vTaskDelay(pdMS_TO_TICKS(200));
    }

    last_play_state = play_state;
    last_next_state = next_state;
    last_prev_state = prev_state;

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

/*********************************
 * PUBLIC FUNCTIONS
 ********************************/
void button_control_init(void) {
  xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);
}
