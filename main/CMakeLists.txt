idf_component_register(SRCS "bt_app_core.c"
                            "main.c"
                    PRIV_REQUIRES bt nvs_flash fatfs sdmmc esp_ringbuf
                    INCLUDE_DIRS ".")
