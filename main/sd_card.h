/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __SD_CARD_H__
#define __SD_CARD_H__

/**
 * @brief Initialize SD card and mount filesystem
 */
void sd_card_init(void);

/**
 * @brief Scan SD card for MP3 files and populate playlist
 */
void sd_card_scan_playlist(void);

/**
 * @brief Get the number of songs in the playlist
 * @return Number of MP3 files found
 */
int sd_card_get_playlist_count(void);

/**
 * @brief Get file path for a specific song index
 * @param index Song index (0 to playlist_count-1)
 * @return Pointer to file path string, or NULL if index is invalid
 */
const char *sd_card_get_file_path(int index);

#endif /* __SD_CARD_H__ */
