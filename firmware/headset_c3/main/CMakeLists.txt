idf_component_register(
    SRCS "main.c" "audio_i2s.c" "link_slave.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_event esp_netif esp_timer esp_adc hs_link
    PRIV_REQUIRES driver
)
