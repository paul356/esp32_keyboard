idf_component_register(SRCS "mk32_main.cpp" "keymap.c" "matrix.c" "keypress_handles.c" "keymap_server.c" "key_definitions.c" "wifi_ap.c"
                    INCLUDE_DIRS "."
                    REQUIRES "battery" "ble" "oled" "nvsfuncs" "plugins" "espnowmk32" "rotencoder" "tinyusb" "esp_http_server" "json")
