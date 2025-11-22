idf_component_register(SRCS "bt_app_core.c"
                            "main.c"
                            "sd_card.c"
                            "button_control.c"
                            "audio_player.c"
                            "bt_gap.c"
                            "bt_a2dp.c"
                            "bt_avrcp.c"
                    PRIV_REQUIRES bt nvs_flash fatfs sdmmc esp_ringbuf
                    INCLUDE_DIRS ".")
