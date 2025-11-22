/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "sd_card.h"
#include "common.h"
#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "gpio_config.h"
#include "sdmmc_cmd.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>

/*********************************
 * CONSTANTS
 ********************************/
#define MOUNT_POINT "/sdcard"
#define MAX_PLAYLIST_SIZE 20
#define MAX_FILENAME_LEN 300

/*********************************
 * STATIC VARIABLES
 ********************************/
static char s_playlist[MAX_PLAYLIST_SIZE][MAX_FILENAME_LEN];
static int s_playlist_count = 0;

/*********************************
 * PUBLIC FUNCTIONS
 ********************************/

void sd_card_init(void) {
  esp_err_t ret;
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024};
  sdmmc_card_t *card;
  const char mount_point[] = MOUNT_POINT;
  ESP_LOGI(BT_AV_TAG, "Initializing SD card");

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.max_freq_khz = 4000; // 4MHz - balance between speed and stability

  spi_bus_config_t bus_cfg = {
      .mosi_io_num = GPIO_SD_MOSI,
      .miso_io_num = GPIO_SD_MISO,
      .sclk_io_num = GPIO_SD_SCLK,
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
  gpio_set_pull_mode(GPIO_SD_MISO, GPIO_PULLUP_ONLY);

  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = GPIO_SD_CS;
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

void sd_card_scan_playlist(void) {
  DIR *dir = opendir(MOUNT_POINT);
  if (!dir) {
    ESP_LOGE(BT_AV_TAG, "Failed to open directory");
    return;
  }

  struct dirent *entry;
  s_playlist_count = 0;

  while ((entry = readdir(dir)) != NULL) {
    if (strstr(entry->d_name, ".mp3") || strstr(entry->d_name, ".MP3")) {
      if (s_playlist_count < MAX_PLAYLIST_SIZE) {
        snprintf(s_playlist[s_playlist_count], MAX_FILENAME_LEN, "%s/%s",
                 MOUNT_POINT, entry->d_name);
        ESP_LOGI(BT_AV_TAG, "Added to playlist: %s",
                 s_playlist[s_playlist_count]);
        s_playlist_count++;
      } else {
        break;
      }
    }
  }

  closedir(dir);
  ESP_LOGI(BT_AV_TAG, "Total songs: %d", s_playlist_count);
}

int sd_card_get_playlist_count(void) { return s_playlist_count; }

const char *sd_card_get_file_path(int index) {
  if (index < 0 || index >= s_playlist_count) {
    return NULL;
  }
  return s_playlist[index];
}
