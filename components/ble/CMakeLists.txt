idf_component_register(SRCS "hal_ble.c" "ble_gap.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "private_include"
                       REQUIRES bt esp_hid)
