idf_component_register(SRCS "bt_app_core.c"
                            "main.c"
                            "sd_card.c"
                            "button_control.c"
                            "audio_player.c"
                            "bt_gap.c"
                            "bt_a2dp.c"
                            "bt_avrcp.c"
                            "oled_display.c"
                            "ssd1306_drv.c"
                    PRIV_REQUIRES bt nvs_flash fatfs sdmmc esp_ringbuf driver esp_lcd
                    INCLUDE_DIRS ".")
