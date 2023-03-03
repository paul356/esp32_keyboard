idf_component_register(SRCS "mk32_main.cpp" "matrix.c" "wifi_ap.c"
                    INCLUDE_DIRS "."
                    REQUIRES quantum tmk_core ble web tinyusb)
