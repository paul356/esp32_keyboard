idf_component_register(SRCS "hal_ble.c" "ble_gap.c" "layout_service.c" "gatts_event_handler.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "private_include"
                       REQUIRES bt esp_hid)
