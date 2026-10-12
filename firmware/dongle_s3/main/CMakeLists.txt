idf_component_register(
    SRCS "main.c" "usb_audio.c" "link_master.c" "tusb/usb_descriptors.c"
    INCLUDE_DIRS "." "tusb"
    REQUIRES nvs_flash esp_wifi esp_event esp_netif esp_timer hs_link
    PRIV_REQUIRES usb
)

idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} BEFORE PUBLIC "${COMPONENT_DIR}/tusb")
target_link_options(${COMPONENT_LIB} INTERFACE
    "-Wl,-u,tud_descriptor_device_cb"
    "-Wl,-u,tud_descriptor_configuration_cb"
    "-Wl,-u,tud_descriptor_string_cb"
    "-Wl,-u,tud_hid_descriptor_report_cb")
