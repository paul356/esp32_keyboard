idf_component_register(SRCS "ble_device.c" "ble_gap.c" "layout_service.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "private_include"
                       REQUIRES bt esp_hid keymap)
