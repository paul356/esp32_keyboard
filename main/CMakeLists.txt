idf_component_register(SRCS "mk32_main.cpp" "keymap.c" "keymap_server.c" "key_definitions.c" "wifi_ap.c"
                    INCLUDE_DIRS "."
                    REQUIRES "quantum" "ble" "nvsfuncs" "hidkeycodeconv" "tinyusb" "esp_http_server" "json")
