idf_component_register(SRCS "hal_ble.c"
                       INCLUDE_DIRS "include" "${CMAKE_SOURCE_DIR}/main"
                       REQUIRES bt esp_hid)
