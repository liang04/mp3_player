/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __AUDIO_PLAYER_H__
#define __AUDIO_PLAYER_H__

#include <stdint.h>

/**
 * @brief Initialize audio player subsystem
 *
 * Creates ringbuffer, initializes MP3 decoder, and starts decode task
 */
void audio_player_init(void);

/**
 * @brief Get PCM audio data for A2DP streaming
 *
 * @param data Buffer to fill with PCM data
 * @param len Length of buffer in bytes
 * @return Number of bytes written to buffer
 */
int32_t audio_player_get_data(uint8_t *data, int32_t len);

/**
 * @brief Set audio volume level
 *
 * @param volume Volume level (0-127, AVRCP standard range)
 */
void audio_player_set_volume(uint8_t volume);

/**
 * @brief Get current volume level
 *
 * @return Current volume (0-127)
 */
uint8_t audio_player_get_volume(void);

#endif /* __AUDIO_PLAYER_H__ */
